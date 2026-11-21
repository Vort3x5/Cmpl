#define NOB_IMPLEMENTATION
#include "include/nob.h"

#include <stdio.h>
#include <string.h>

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
    /* strip trailing newline */
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

/* Headless + Wayland/XCB Vulkan helpers for BasicJai examples.
 * Headers/libs: pkg-config (nix-shell / distro), not FHS /usr/lib scans. */
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

    /* Prefer system/nix vulkan-headers via pkg-config; fall back to vendored copy. */
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

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (argc < 2) {
        nob_log(NOB_INFO, "Usage: %s <command>", argv[0]);
        nob_log(NOB_INFO, "  all  — build out/cmpl + examples/vulkan/libcmpl_vk.so");
        nob_log(NOB_INFO, "  vk   — build only libcmpl_vk.so");
        return 1;
    }

    const char *command = argv[1];

    if (strcmp(command, "all") == 0) {
        bool ok = BuildAll();
        ok = BuildVulkanSo() && ok;
        return !ok;
    } else if (strcmp(command, "vk") == 0) {
        return !BuildVulkanSo();
    } else {
        nob_log(NOB_ERROR, "Unknown command: %s", command);
        return 1;
    }

    return 0;
}
