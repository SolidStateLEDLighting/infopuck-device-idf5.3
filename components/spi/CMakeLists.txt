#
FILE(GLOB_RECURSE SOURCES src/spi/*.cpp)
#
# Exposes components to both source and header files.
set(REQUIRES
     esp_driver_spi
)
#
# Anything that must be exposed to the sources files, but may remain hidden from the header files.
set(PRIV_REQUIRES
     main
     nvs
)
#
#
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "include"
                       REQUIRES ${REQUIRES}
                       PRIV_REQUIRES ${PRIV_REQUIRES}
                      )
