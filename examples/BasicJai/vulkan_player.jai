// Milestone 3: moving player — position + loop in Jai, thin C session API.
//
//   nix-shell
//   ./nob all
//   ./out/cmpl examples/BasicJai/vulkan_player.jai
//   CMPL_VK_FRAMES=120 ./out/out; echo $?
//
// WASD / arrows move. Auto-drift proves motion without keyboard.
// Keys: 0=left 1=right 2=up 3=down

vk :: #library "examples/vulkan/libcmpl_vk.so";

cmpl_vk_game_open  :: () -> int #foreign vk;
cmpl_vk_game_poll  :: () -> int #foreign vk;
cmpl_vk_game_key   :: (k: int) -> int #foreign vk;
cmpl_vk_game_draw  :: (x: int, y: int) -> int #foreign vk;
cmpl_vk_game_close :: () -> int #foreign vk;

main :: () -> int {
    err := cmpl_vk_game_open();
    if err != 0 {
        return err;
    }

    x := 0;
    y := 0;
    alive := 1;

    while alive > 0 {
        alive = cmpl_vk_game_poll();
        if alive > 0 {
            if cmpl_vk_game_key(0) > 0 {
                x = x - 8;
            }
            if cmpl_vk_game_key(1) > 0 {
                x = x + 8;
            }
            if cmpl_vk_game_key(2) > 0 {
                y = y - 8;
            }
            if cmpl_vk_game_key(3) > 0 {
                y = y + 8;
            }

            // Auto-drift so the milestone is visible without input
            x = x + 2;
            if x > 160 {
                x = 0 - 160;
            }

            alive = cmpl_vk_game_draw(x, y);
        }
    }

    cmpl_vk_game_close();
    return 0;
}
