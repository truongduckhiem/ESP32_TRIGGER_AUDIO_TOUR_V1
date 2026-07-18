# File: components/dfplayer/CMakeLists.txt
idf_component_register(SRCS "dfplayer.c"
                      INCLUDE_DIRS "include"
                      REQUIRES driver
)