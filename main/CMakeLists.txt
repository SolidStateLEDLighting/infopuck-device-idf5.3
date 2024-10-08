#
# Use the Global Recursive action to gather up all the source files under a variable called SOURCES
#
# NOTE: Anytime you add a new .cpp files, the entire project must be cleaned and rebuilt or the new files
# will not be included in the compilation process.  This is the one drawback of GLOB_RECURSE implicit inclusion.
#
FILE(GLOB_RECURSE SOURCES ${PROJECT_DIR}/main/src/*.cpp)
#
# Included components which are exposed in public header files.
set(REQUIRES
    esp_system
    esp_timer
    driver
    display
    i2c
    spi
    wifi
)
#
# Anything that must be included, but may remain hidden from the public header files.
# Limiting component scope can reduce Undefined Reference linkage problems in large applications.
set(PRIV_REQUIRES
    app_update
    esp_event
    esp_netif
    nvs
)
#
#
set(COMPONENT_ADD_INCLUDEDIRS
    ${CMAKE_CURRENT_LIST_DIR}/include/system
    ${CMAKE_CURRENT_LIST_DIR}/include/logging
)
#
#
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
                       REQUIRES ${REQUIRES}
                       PRIV_REQUIRES ${PRIV_REQUIRES}
)