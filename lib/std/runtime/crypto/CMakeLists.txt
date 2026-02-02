# TML Crypto Runtime - CMake Build Configuration
#
# This builds the crypto runtime library with platform-specific implementations.

cmake_minimum_required(VERSION 3.16)

# Source files common to all platforms
set(CRYPTO_COMMON_SOURCES
    crypto_common.c
    crypto_random.c
)

# Platform-specific source files
if(WIN32)
    set(CRYPTO_PLATFORM_SOURCES
        crypto_hash_win.c
        crypto_cipher_win.c
        crypto_kdf_win.c
        # crypto_sign_win.c
        # crypto_key_win.c
        # crypto_x509_win.c
        # crypto_dh_win.c
        # crypto_ecdh_win.c
        # crypto_rsa_win.c
    )
    set(CRYPTO_PLATFORM_LIBS
        bcrypt
        crypt32
        ncrypt
    )
elseif(APPLE)
    set(CRYPTO_PLATFORM_SOURCES
        crypto_hash_macos.c
        # crypto_cipher_macos.c
        # crypto_kdf_macos.c
        # crypto_sign_macos.c
        # crypto_key_macos.c
        # crypto_x509_macos.c
        # crypto_dh_macos.c
        # crypto_ecdh_macos.c
        # crypto_rsa_macos.c
    )
    set(CRYPTO_PLATFORM_LIBS
        "-framework Security"
        "-framework CoreFoundation"
    )
else()
    # Linux and other Unix-like systems use OpenSSL
    find_package(OpenSSL REQUIRED)

    set(CRYPTO_PLATFORM_SOURCES
        crypto_hash_openssl.c
        # crypto_cipher_openssl.c
        # crypto_kdf_openssl.c
        # crypto_sign_openssl.c
        # crypto_key_openssl.c
        # crypto_x509_openssl.c
        # crypto_dh_openssl.c
        # crypto_ecdh_openssl.c
        # crypto_rsa_openssl.c
    )
    set(CRYPTO_PLATFORM_LIBS
        OpenSSL::SSL
        OpenSSL::Crypto
    )
endif()

# Create the crypto runtime library
add_library(tml_crypto STATIC
    ${CRYPTO_COMMON_SOURCES}
    ${CRYPTO_PLATFORM_SOURCES}
)

target_include_directories(tml_crypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tml_crypto PRIVATE
    ${CRYPTO_PLATFORM_LIBS}
)

# Set compile options
if(MSVC)
    target_compile_options(tml_crypto PRIVATE /W4)
else()
    target_compile_options(tml_crypto PRIVATE -Wall -Wextra -pedantic)
endif()

# Optional: Link with libsodium for additional algorithms (BLAKE2, Argon2, etc.)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM QUIET libsodium)
    if(SODIUM_FOUND)
        target_compile_definitions(tml_crypto PRIVATE TML_HAVE_LIBSODIUM)
        target_include_directories(tml_crypto PRIVATE ${SODIUM_INCLUDE_DIRS})
        target_link_libraries(tml_crypto PRIVATE ${SODIUM_LIBRARIES})
    endif()
endif()

# Optional: Link with Argon2 library
if(PkgConfig_FOUND)
    pkg_check_modules(ARGON2 QUIET libargon2)
    if(ARGON2_FOUND)
        target_compile_definitions(tml_crypto PRIVATE TML_HAVE_ARGON2)
        target_include_directories(tml_crypto PRIVATE ${ARGON2_INCLUDE_DIRS})
        target_link_libraries(tml_crypto PRIVATE ${ARGON2_LIBRARIES})
    endif()
endif()

# Export the library
install(TARGETS tml_crypto
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    ../crypto.h
    crypto_internal.h
    DESTINATION include/tml
)