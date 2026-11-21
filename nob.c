#define NOB_IMPLEMENTATION
#include "include/nob.h"

static const char *FindLibVulkan(void)
{
    static const char *candidates[] = {
        "/usr/lib/libvulkan.so",
        "/usr/lib64/libvulkan.so",
        "/lib/libvulkan.so",
        "/lib64/libvulkan.so",
    };
    for (size_t i = 0; i < NOB_ARRAY_LEN(candidates); i++) {
        if (nob_file_exists(candidates[i]) > 0)
            return candidates[i];
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

/* Headless Vulkan helper used by examples/BasicJai/vulkan_triangle.jai.
 * Prefer an absolute libvulkan.so path: Nix ld often fails -lvulkan without
 * a nix-shell (NIX_LDFLAGS). Absolute path bypasses the search. */
bool BuildVulkanSo(void)
{
    const char *out = "examples/vulkan/libcmpl_vk.so";
    const char *src = "examples/vulkan/cmpl_vk.c";
    const char *inputs[] = { src };

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
                   "-o", out, src);

    const char *vk = FindLibVulkan();
    if (vk) {
        nob_log(NOB_INFO, "Linking Vulkan via %s", vk);
        nob_cmd_append(&cmd, vk);
    } else {
        nob_log(NOB_INFO, "libvulkan.so not found in /usr|/lib — trying -lvulkan (use nix-shell / nix develop)");
        nob_cmd_append(&cmd, "-lvulkan");
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
