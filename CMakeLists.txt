cmake_minimum_required(VERSION 3.22)
set(SC_MODULE phplib)
project(simply-cpp-${SC_MODULE} HOMEPAGE_URL "https://github.com/roelofrossouw/simply-cpp-phplib" LANGUAGES CXX)
include(cmake/sc_bootstrap.cmake)

option(SC_PHPLIB_BUILD_TESTS "Build simply-cpp-phplib tests" ${PROJECT_IS_TOP_LEVEL})
# Installing writes into PHP's own directories. On macOS those belong to Homebrew, and install.sh
# installs with sudo, so it is off there by default.
if (APPLE)
    option(SC_PHPLIB_INSTALL "Install the extension into PHP's directories" OFF)
else ()
    option(SC_PHPLIB_INSTALL "Install the extension into PHP's directories" ON)
endif ()

get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

################
# Dependencies #
################
# The extension is a thin wrapper: what it wraps comes from these modules' shared libraries (their
# runtime packages), not copied into it.
find_or_install_package(sc-core simply-cpp-dev simply-cpp 1.14.1)
find_or_install_package(sc-image simply-cpp-image-dev simply-cpp-image 5.1.5)
find_or_install_package(sc-html2pdf simply-cpp-html2pdf-dev simply-cpp-html2pdf 1.2.0)

## PHP ##
# An extension is built for one PHP version: the one each Ubuntu release ships (jammy 8.1, noble
# 8.3, resolute 8.5), through its php-config; elsewhere (macOS) Homebrew's. SC_PHPLIB_PHP_CONFIG
# chooses another.
find_program(SC_PHPLIB_PHP_CONFIG NAMES php-config)
if (NOT SC_PHPLIB_PHP_CONFIG AND NOT APPLE)
    message(STATUS "No php-config found, installing php-dev and php-cli")
    execute_process(COMMAND sudo apt-get install -y php-dev php-cli)
    unset(SC_PHPLIB_PHP_CONFIG CACHE)
    find_program(SC_PHPLIB_PHP_CONFIG NAMES php-config)
endif ()
if (NOT SC_PHPLIB_PHP_CONFIG)
    message(FATAL_ERROR "No php-config found. Install php-dev (apt) or php (brew), or pass -DSC_PHPLIB_PHP_CONFIG=/path/to/php-config")
endif ()
function(sc_phplib_php_config option output)
    execute_process(COMMAND "${SC_PHPLIB_PHP_CONFIG}" ${option}
            OUTPUT_VARIABLE value OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    set(${output} "${value}" PARENT_SCOPE)
endfunction()
sc_phplib_php_config(--version SC_PHPLIB_PHP_VERSION)
sc_phplib_php_config(--includes SC_PHPLIB_PHP_INCLUDES)
sc_phplib_php_config(--extension-dir SC_PHPLIB_PHP_EXTENSION_DIR)
sc_phplib_php_config(--php-binary SC_PHPLIB_PHP_BINARY)
string(REGEX MATCH "^[0-9]+\\.[0-9]+" SC_PHPLIB_PHP_MINOR "${SC_PHPLIB_PHP_VERSION}")
# PHP's module API, which an extension must match: the extension directory's name (20250925).
get_filename_component(SC_PHPLIB_PHP_API "${SC_PHPLIB_PHP_EXTENSION_DIR}" NAME)
separate_arguments(SC_PHPLIB_PHP_INCLUDES UNIX_COMMAND "${SC_PHPLIB_PHP_INCLUDES}")
message(STATUS "Building for PHP ${SC_PHPLIB_PHP_VERSION} (module API ${SC_PHPLIB_PHP_API}, ${SC_PHPLIB_PHP_CONFIG})")

#############
# Extension #
#############
# sc.so, loaded by PHP with dlopen(): no "lib" prefix, no link against PHP (its symbols resolve at
# load time; macOS needs that said). It links the modules' shared libraries themselves, not their
# CMake targets, which would also bring the static libraries under them into it.
add_library(sc MODULE src/sc_php.cpp src/base64.cpp src/svg.cpp src/html2pdf.cpp src/php_glue.h)
target_compile_options(sc PRIVATE ${SC_PHPLIB_PHP_INCLUDES})
target_compile_definitions(sc PRIVATE SC_PHPLIB_VERSION="${SC_VERSION}")
foreach (module sc-core sc-image sc-html2pdf)
    target_link_libraries(sc PRIVATE $<TARGET_LINKER_FILE:sc::${module}-shared>)
    target_include_directories(sc PRIVATE $<TARGET_PROPERTY:sc::${module}-shared,INTERFACE_INCLUDE_DIRECTORIES>)
endforeach ()
set_target_properties(sc PROPERTIES PREFIX "" SUFFIX ".so")
if (APPLE)
    target_link_options(sc PRIVATE -undefined dynamic_lookup)
endif ()

# Turned on the Debian way: mods-available/sc.ini, enabled for every SAPI by phpenmod.
configure_file(sc.ini.in sc.ini @ONLY)
# Stubs for IDEs (PhpStorm, Intelephense): what the extension declares, documented.
install(FILES stubs/sc.stub.php DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/simply-cpp-phplib COMPONENT runtime)
if (SC_PHPLIB_INSTALL)
    install(TARGETS sc LIBRARY DESTINATION "${SC_PHPLIB_PHP_EXTENSION_DIR}" COMPONENT runtime)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sc.ini" DESTINATION "/etc/php/${SC_PHPLIB_PHP_MINOR}/mods-available" COMPONENT runtime)
endif ()

#########
# Tests #
#########
if (SC_PHPLIB_BUILD_TESTS)
    enable_testing()
    # Each tests/*.php run by the PHP it was built for, with only this extension loaded (-n).
    file(GLOB SC_PHPLIB_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.php)
    foreach (test ${SC_PHPLIB_TESTS})
        get_filename_component(name "${test}" NAME_WE)
        add_test(NAME test-php-${name}
                COMMAND "${SC_PHPLIB_PHP_BINARY}" -n -d "extension=$<TARGET_FILE:sc>" "${test}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        set_tests_properties(test-php-${name} PROPERTIES LABELS unit FAIL_REGULAR_EXPRESSION "FAILED")
    endforeach ()
endif ()

#############
# Packaging #
#############
# Enabled for PHP's SAPIs on install and php-fpm restarted (when it runs), the same on upgrade
# and removal.
foreach (script postinst prerm postrm)
    configure_file(packaging/${script}.in packaging/${script} @ONLY FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach ()
set(CPACK_DEBIAN_RUNTIME_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_BINARY_DIR}/packaging/postinst;${CMAKE_CURRENT_BINARY_DIR}/packaging/prerm;${CMAKE_CURRENT_BINARY_DIR}/packaging/postrm")
# phpapi-<api> is what Ubuntu's PHP provides for its module API: the package can't go into another.
# The modules' packages are named, not left to dpkg-shlibdeps: on a build server the libraries it
# links are the deploys' own copies in /usr/local/lib, which no package owns.
package_sc_module(DEPENDS "phpapi-${SC_PHPLIB_PHP_API}" "php${SC_PHPLIB_PHP_MINOR}-common"
        "simply-cpp (>= 1.14.1~)" "simply-cpp-image (>= 5.1.5~)" "simply-cpp-html2pdf (>= 1.1.0~)")
