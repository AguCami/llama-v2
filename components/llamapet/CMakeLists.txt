idf_component_register(
    SRCS "src/pet.c"
         "src/llama_model.c"
         "src/llama_scene.c"
         "src/game.c"
         "src/ui.c"
         "src/minigame.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES g3d)
