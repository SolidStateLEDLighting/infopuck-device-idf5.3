#
FILE(GLOB_RECURSE SOURCES src/*.cpp)
#
# Exposes components to both source and header files.
set(DISPLAY_REQUIRES
)
#
#
# Anything that must be exposed to the sources files, but may remain hidden from the header files.
set(DISPLAY_PRIV_REQUIRES
    main
)
#
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "include"
                       REQUIRES ${DISPLAY_REQUIRES}
                       PRIV_REQUIRES ${DISPLAY_PRIV_REQUIRES}
                      )