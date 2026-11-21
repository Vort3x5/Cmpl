#define NOB_IMPLEMENTATION
#include "include/nob.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* Append space-separated flags from `pkg-config --cflags|--libs <pkg>` if available.
 * On Nix, NIX_CFLAGS_COMPILE / NIX_LDFLAGS already provide search paths for -l… */
static void AppendPkgConfig(Nob_Cmd *cmd, const char *pkg, bool cflags)
{
    char cmdline[256];
    snprintf(cmdline, sizeof(cmdline), "pkg-config --%s %s",
             cflags ? "cflags" : "libs", pkg);
    FILE *fp = popen(cmdline, "r");
    if (!fp)
        return;
    char buf[1024];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return;
    }
    int st = pclose(fp);
    if (st != 0)
        return;
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
        nob_cmd_append(cmd, nob_temp_sprintf("%s", start));
    }
}

bool BuildAll(void)
{
    nob_mkdir_if_not_exists("out");

    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-g", "-I", "include", "-Wno-misleading-indentation");
    nob_cc_output(&cmd, "out/cmpl");

    nob_cmd_append(&cmd, "src/main.c");
    nob_cmd_append(&cmd, "src/lexer.c");
    nob_cmd_append(&cmd, "src/parser.c");
    nob_cmd_append(&cmd, "src/generator.c");
    nob_cmd_append(&cmd, "src/tac.c");

    return nob_cmd_run(&cmd);
}

bool BuildVulkanSo(void)
{
    const char *out = "examples/vulkan/libcmpl_vk.so";
    const char *inputs[] = {
        "examples/vulkan/cmpl_vk.c",
        "examples/vulkan/cmpl_vk_present.c",
        "examples/vulkan/cmpl_vk_wl.c",
        "examples/vulkan/cmpl_vk_xcb.c",
        "examples/vulkan/cmpl_vk_game.c",
        "examples/vulkan/wayland/xdg-shell-protocol.c",
    };

    int rebuild = nob_needs_rebuild(out, inputs, NOB_ARRAY_LEN(inputs));
    if (rebuild < 0)
        return false;
    if (rebuild == 0) {
        nob_log(NOB_INFO, "`%s` is up to date", out);
        return true;
    }

    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cmd_append(&cmd, "-shared", "-fPIC", "-O2",
                   "-I", "examples/vulkan",
                   "-o", out,
                   inputs[0], inputs[1], inputs[2], inputs[3], inputs[4], inputs[5]);

    int have_pc = (system("pkg-config --exists vulkan 2>/dev/null") == 0);
    if (have_pc) {
        nob_log(NOB_INFO, "Vulkan via pkg-config (nix/system headers)");
        AppendPkgConfig(&cmd, "vulkan", true);
        AppendPkgConfig(&cmd, "vulkan", false);
        AppendPkgConfig(&cmd, "wayland-client", true);
        AppendPkgConfig(&cmd, "wayland-client", false);
        AppendPkgConfig(&cmd, "xcb", true);
        AppendPkgConfig(&cmd, "xcb", false);
    } else {
        nob_log(NOB_INFO, "pkg-config vulkan missing — vendored headers + -l flags (use nix-shell)");
        nob_cmd_append(&cmd, "-I", "examples/vulkan/include");
        nob_cmd_append(&cmd, "-lvulkan", "-lwayland-client", "-lxcb");
    }

    if (!nob_cmd_run(&cmd)) {
        nob_log(NOB_ERROR, "Failed to build %s", out);
        nob_log(NOB_ERROR, "Enter the Nix env first:  nix-shell  OR  nix develop");
        return false;
    }
    return true;
}

static bool EnsureCLibs(void)
{
    struct { const char *so; const char *src; } libs[] = {
        { "examples/c_lib/libadd.so", "examples/c_lib/add.c" },
        { "examples/c_lib/libfval.so", "examples/c_lib/fval.c" },
    };
    for (size_t i = 0; i < NOB_ARRAY_LEN(libs); i++) {
        int need = nob_needs_rebuild(libs[i].so, (const char*[]){ libs[i].src }, 1);
        if (need < 0) return false;
        if (need == 0) continue;
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, "cc", "-shared", "-fPIC", "-o", libs[i].so, libs[i].src);
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "Failed to build %s", libs[i].so);
            return false;
        }
    }
    return true;
}

/* Minimal tests.json walker: objects with "file", "exit", optional "nix": true. */
static bool RunTests(void)
{
    if (!BuildAll()) return false;
    if (!EnsureCLibs()) return false;

    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file("tests.json", &sb)) {
        nob_log(NOB_ERROR, "Could not read tests.json");
        return false;
    }
    nob_sb_append_null(&sb);

    bool have_vulkan = (system("pkg-config --exists vulkan 2>/dev/null") == 0);
    int passed = 0, failed = 0, skipped = 0;
    const char *p = sb.items;

    while ((p = strstr(p, "\"file\"")) != NULL) {
        p += 6;
        while (*p && *p != '"') p++;
        if (*p != '"') break;
        p++;
        const char *file_start = p;
        while (*p && *p != '"') p++;
        if (*p != '"') break;
        char file[512];
        size_t flen = (size_t)(p - file_start);
        if (flen >= sizeof(file)) flen = sizeof(file) - 1;
        memcpy(file, file_start, flen);
        file[flen] = '\0';
        p++;

        const char *exit_key = strstr(p, "\"exit\"");
        const char *next_file = strstr(p, "\"file\"");
        if (!exit_key || (next_file && exit_key > next_file)) {
            nob_log(NOB_ERROR, "tests.json: missing exit for %s", file);
            failed++;
            continue;
        }
        exit_key += 6;
        while (*exit_key == ' ' || *exit_key == '\t' || *exit_key == ':') exit_key++;
        int expected = (int)strtol(exit_key, NULL, 10);

        bool needs_nix = false;
        const char *nix_key = strstr(p, "\"nix\"");
        if (nix_key && (!next_file || nix_key < next_file)) {
            const char *t = strstr(nix_key, "true");
            if (t && (!next_file || t < next_file))
                needs_nix = true;
        }

        if (needs_nix && !have_vulkan) {
            nob_log(NOB_INFO, "SKIP %s (needs nix/pkg-config vulkan)", file);
            skipped++;
            continue;
        }

        const char *exe = nob_temp_sprintf("out/test_%d", passed + failed + skipped);
        Nob_Cmd compile = {0};
        nob_cmd_append(&compile, "./out/cmpl", file, exe);
        /* Quiet AST dump: cmpl always prints; we only care about exit. */
        if (!nob_cmd_run(&compile)) {
            nob_log(NOB_ERROR, "FAIL %s (compile)", file);
            failed++;
            continue;
        }

        Nob_Cmd run = {0};
        nob_cmd_append(&run, exe);
        /* nob_cmd_run waits; capture exit via system for status */
        int st = system(exe);
        int got = -1;
        if (WIFEXITED(st))
            got = WEXITSTATUS(st);
        if (got == expected) {
            nob_log(NOB_INFO, "OK   %s (exit %d)", file, got);
            passed++;
        } else {
            nob_log(NOB_ERROR, "FAIL %s (exit %d, expected %d)", file, got, expected);
            failed++;
        }
    }

    nob_log(NOB_INFO, "Tests: %d passed, %d failed, %d skipped", passed, failed, skipped);
    nob_sb_free(sb);
    return failed == 0;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (argc < 2) {
        nob_log(NOB_INFO, "Usage: %s <command>", argv[0]);
        nob_log(NOB_INFO, "  all   — build out/cmpl + examples/vulkan/libcmpl_vk.so");
        nob_log(NOB_INFO, "  vk    — build only libcmpl_vk.so");
        nob_log(NOB_INFO, "  test  — build cmpl + run tests.json");
        return 1;
    }

    const char *command = argv[1];

    if (strcmp(command, "all") == 0) {
        bool ok = BuildAll();
        ok = BuildVulkanSo() && ok;
        return !ok;
    } else if (strcmp(command, "vk") == 0) {
        return !BuildVulkanSo();
    } else if (strcmp(command, "test") == 0) {
        return !RunTests();
    } else {
        nob_log(NOB_ERROR, "Unknown command: %s", command);
        return 1;
    }

    return 0;
}
