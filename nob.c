#define NOB_IMPLEMENTATION
#include "include/nob.h"

/* Caller owns the returned string (nob_temp lifetime). */
static const char *FindSharedLib(const char *name)
{
    static const char *dirs[] = { "/usr/lib", "/usr/lib64", "/lib", "/lib64" };
    for (size_t i = 0; i < NOB_ARRAY_LEN(dirs); i++) {
        const char *path = nob_temp_sprintf("%s/%s", dirs[i], name);
        if (nob_file_exists(path) > 0)
            return path;
    }
    return NULL;
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

/* Headless + Wayland Vulkan helpers for BasicJai examples. */
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
                   "-I", "examples/vulkan/include",
                   "-I", "examples/vulkan",
                   "-o", out,
                   inputs[0], inputs[1], inputs[2], inputs[3], inputs[4], inputs[5]);

    const char *vk = FindSharedLib("libvulkan.so");
    if (vk) {
        nob_log(NOB_INFO, "Linking Vulkan via %s", vk);
        nob_cmd_append(&cmd, vk);
    } else {
        nob_log(NOB_INFO, "libvulkan.so not in /usr|/lib — trying -lvulkan (use nix-shell)");
        nob_cmd_append(&cmd, "-lvulkan");
    }

    const char *wl = FindSharedLib("libwayland-client.so");
    if (wl) {
        nob_log(NOB_INFO, "Linking Wayland via %s", wl);
        nob_cmd_append(&cmd, wl);
    } else {
        nob_log(NOB_INFO, "libwayland-client.so not found — trying -lwayland-client");
        nob_cmd_append(&cmd, "-lwayland-client");
    }

    const char *xcb = FindSharedLib("libxcb.so");
    if (xcb) {
        nob_log(NOB_INFO, "Linking XCB via %s", xcb);
        nob_cmd_append(&cmd, xcb);
    } else {
        nob_log(NOB_INFO, "libxcb.so not found — trying -lxcb");
        nob_cmd_append(&cmd, "-lxcb");
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
}
