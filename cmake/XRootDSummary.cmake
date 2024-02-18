#-------------------------------------------------------------------------------
# Print the configuration summary
#-------------------------------------------------------------------------------
set( TRUE_VAR TRUE )
component_status( CEPH      ENABLE_CEPH       BUILD_CEPH )
component_status( FUSE      BUILD_FUSE        FUSE_FOUND )
component_status( HTTP      BUILD_HTTP        OPENSSL_FOUND )
component_status( KRB5      BUILD_KRB5        KERBEROS5_FOUND )
component_status( MACAROONS BUILD_MACAROONS   MACAROONS_FOUND AND JSON_FOUND AND BUILD_HTTP )
component_status( PYTHON    BUILD_PYTHON      Python_Interpreter_FOUND AND Python_Development_FOUND )
component_status( READLINE  ENABLE_READLINE   READLINE_FOUND )
component_status( SCITOKENS BUILD_SCITOKENS   SCITOKENSCPP_FOUND )
component_status( SERVER_TESTS ENABLE_SERVER_TESTS TRUE_VAR )
component_status( TESTS     BUILD_TESTS       GTEST_FOUND )
component_status( TPC       BUILD_TPC         CURL_FOUND )
component_status( VOMSXRD   BUILD_VOMS        VOMS_FOUND )
component_status( XRDCL     ENABLE_XRDCL      TRUE_VAR )
component_status( XRDCLHTTP ENABLE_XRDCLHTTP  DAVIX_FOUND )
component_status( XRDEC     BUILD_XRDEC       TRUE_VAR )

message( STATUS "----------------------------------------" )
message( STATUS "Installation path: " ${CMAKE_INSTALL_PREFIX} )
message( STATUS "C Compiler:        " ${CMAKE_C_COMPILER} )
message( STATUS "C++ Compiler:      " ${CMAKE_CXX_COMPILER} )
message( STATUS "Build type:        " ${CMAKE_BUILD_TYPE} )
message( STATUS "Plug-in version:   " ${PLUGIN_VERSION} )
message( STATUS "" )
message( STATUS "Ceph support:      " ${STATUS_CEPH} )
message( STATUS "Readline support:  " ${STATUS_READLINE} )
message( STATUS "FUSE support:      " ${STATUS_FUSE} )
message( STATUS "Kerberos5 support: " ${STATUS_KRB5} )
message( STATUS "XrdCl:             " ${STATUS_XRDCL} )
message( STATUS "XrdClHttp:         " ${STATUS_XRDCLHTTP} )
message( STATUS "HTTP support:      " ${STATUS_HTTP} )
message( STATUS "HTTP TPC support:  " ${STATUS_TPC} )
message( STATUS "VOMS support:      " ${STATUS_VOMSXRD} )
message( STATUS "Python support:    " ${STATUS_PYTHON} )
message( STATUS "Erasure coding:    " ${STATUS_XRDEC} )
message( STATUS "Macaroons:         " ${STATUS_MACAROONS} )
message( STATUS "SciTokens:         " ${STATUS_SCITOKENS} )
message( STATUS "Tests:             " ${STATUS_TESTS} )
message( STATUS "Server Tests:      " ${STATUS_SERVER_TESTS} )
message( STATUS "----------------------------------------" )

if( FORCE_ENABLED )
  foreach(FEATURE CEPH FUSE HTTP KRB5 MACAROONS PYTHON READLINE SCITOKENS TESTS VOMSXRD XRDCL XRDCLHTTP)
    if(ENABLE_${FEATURE} AND NOT STATUS_${FEATURE} STREQUAL "yes")
      message(SEND_ERROR "Could not enable feature: ${FEATURE}")
    endif()
  endforeach()
endif()
