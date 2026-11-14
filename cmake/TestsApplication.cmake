if(BUILD_TESTING)
    vellum_test_executable(vellum_pdf_tests tests/test_pdf.cc)
    target_link_libraries(vellum_pdf_tests PRIVATE vellum_pdf)
    execute_process(COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/make_fixture.py
        ${PROJECT_BINARY_DIR}/reference.pdf COMMAND_ERROR_IS_FATAL ANY)
    add_test(NAME pdf.render_copy_validate COMMAND vellum_pdf_tests ${PROJECT_BINARY_DIR}/reference.pdf)
    vellum_test_executable(vellum_mupdf_callback_guard_tests tests/test_mupdf_callback_guard.cc)
    target_link_libraries(vellum_mupdf_callback_guard_tests PRIVATE vellum_mupdf)
    add_test(NAME pdf.callback_guard COMMAND vellum_mupdf_callback_guard_tests)
    vellum_test_executable(vellum_pdf_disk_full_tests tests/test_pdf_disk_full.cc)
    target_link_libraries(vellum_pdf_disk_full_tests PRIVATE vellum_pdf)
    add_test(NAME pdf.disk_full COMMAND ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/scripts/run_disk_full_test.py $<TARGET_FILE:vellum_pdf_disk_full_tests>)
    set_tests_properties(pdf.disk_full PROPERTIES TIMEOUT 120 RESOURCE_LOCK vellum_disk_io
        ENVIRONMENT "VELLUM_DISK_FULL_REFERENCE=${PROJECT_BINARY_DIR}/reference.pdf;VELLUM_DISK_FULL_EXISTING=${PROJECT_SOURCE_DIR}/tests/fixtures/restricted.pdf")
    vellum_test_executable(vellum_pdf_allocation_fault_tests tests/test_pdf_allocation_faults.cc)
    target_link_libraries(vellum_pdf_allocation_fault_tests PRIVATE vellum_pdf)
    add_test(NAME pdf.allocation_failures COMMAND vellum_pdf_allocation_fault_tests ${PROJECT_BINARY_DIR}/reference.pdf)
    set_tests_properties(pdf.allocation_failures PROPERTIES TIMEOUT 120 RESOURCE_LOCK vellum_disk_io)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        vellum_test_executable(vellum_pdf_mupdf_fault_tests tests/test_pdf_mupdf_faults.cc)
        target_link_libraries(vellum_pdf_mupdf_fault_tests PRIVATE vellum_pdf vellum_mupdf)
        target_link_options(vellum_pdf_mupdf_fault_tests PRIVATE
            "-Wl,--wrap=fz_new_context_imp,--wrap=fz_load_page,--wrap=fz_bound_page,--wrap=fz_has_permission,--wrap=pdf_authenticate_password,--wrap=fz_new_pixmap_from_page,--wrap=pdf_new_pixmap_from_page_with_usage,--wrap=fz_new_pixmap_with_bbox,--wrap=fz_new_draw_device,--wrap=pdf_run_page_with_usage,--wrap=fz_close_device")
        target_link_options(vellum_pdf_mupdf_fault_tests PRIVATE
            "-Wl,--wrap=fz_register_document_handlers,--wrap=fz_open_memory,--wrap=fz_open_document_with_stream,--wrap=fz_count_pages,--wrap=pdf_was_repaired,--wrap=pdf_page_from_fz_page,--wrap=pdf_specifics,--wrap=_ZNSt13runtime_errorC1EPKc,--wrap=_ZNSt13runtime_errorC2EPKc,--wrap=_ZNSt16invalid_argumentC1EPKc")
        add_test(NAME pdf.mupdf_boundary_failures COMMAND vellum_pdf_mupdf_fault_tests ${PROJECT_BINARY_DIR}/reference.pdf ${PROJECT_SOURCE_DIR}/tests/fixtures/restricted.pdf)
        set_tests_properties(pdf.mupdf_boundary_failures PROPERTIES TIMEOUT 60)
    endif()
    vellum_test_executable(vellum_permission_tests tests/test_permissions.cc)
    target_link_libraries(vellum_permission_tests PRIVATE vellum_printing vellum_recovery)
    add_test(NAME pdf.permissions_owner_auth COMMAND vellum_permission_tests ${PROJECT_SOURCE_DIR}/tests/fixtures)
    vellum_test_executable(vellum_permission_allocation_fault_tests tests/test_permissions_allocation_faults.cc)
    target_link_libraries(vellum_permission_allocation_fault_tests PRIVATE vellum_pdf)
    add_test(NAME pdf.permission_allocation_failures COMMAND vellum_permission_allocation_fault_tests
        ${PROJECT_SOURCE_DIR}/tests/fixtures/long-owner.pdf)
    set_tests_properties(pdf.permission_allocation_failures PROPERTIES TIMEOUT 120)
    function(vellum_gui_test name)
        add_test(NAME ${name} COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/run_headless_test.py ${ARGN})
    endfunction()
    add_test(NAME testing.headless_runner COMMAND ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/tests/test_headless_runner.py)
    set_tests_properties(testing.headless_runner PROPERTIES TIMEOUT 60 PROCESSORS 2)
    add_test(NAME testing.headless_faults COMMAND ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/tests/test_headless_faults.py)
    set_tests_properties(testing.headless_faults PROPERTIES TIMEOUT 60)
    vellum_test_executable(vellum_ui_tests tests/test_ui.cc)
    target_link_libraries(vellum_ui_tests PRIVATE vellum_ui)
    vellum_gui_test(ui.reader_navigation_editor $<TARGET_FILE:vellum_ui_tests> ${PROJECT_BINARY_DIR}/reference.pdf ${PROJECT_SOURCE_DIR}/tests/fixtures)
    vellum_gui_test(ui.application_startup ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/tests/test_app_startup.py $<TARGET_FILE:vellum>
        ${PROJECT_BINARY_DIR}/reference.pdf)
    set_tests_properties(ui.application_startup PROPERTIES TIMEOUT 40)
    vellum_gui_test(ui.window_discovery_churn ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/tests/test_window_churn.py $<TARGET_FILE:vellum>
        ${PROJECT_BINARY_DIR}/reference.pdf)
    set_tests_properties(ui.window_discovery_churn PROPERTIES TIMEOUT 150)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        vellum_test_executable(vellum_app_allocation_fault_tests tests/test_app_allocation_faults.cc)
        target_link_libraries(vellum_app_allocation_fault_tests PRIVATE vellum_ui)
        target_link_options(vellum_app_allocation_fault_tests PRIVATE
            "-Wl,--wrap=_ZN5wxAppC2Ev,--wrap=_ZN6vellum10main_frameC1Ev,--wrap=_ZN19wxTopLevelWindowGTK4ShowEb,--wrap=_ZN6vellum10main_frame9open_pathERKNSt10filesystem7__cxx114pathE")
        foreach(failure allocation constructor application app-constructor)
            vellum_gui_test(ui.application_${failure}_failure $<TARGET_FILE:vellum_app_allocation_fault_tests>)
            set_tests_properties(ui.application_${failure}_failure PROPERTIES TIMEOUT 30
                ENVIRONMENT "VELLUM_TEST_APP_FAILURE=${failure}")
        endforeach()
        foreach(index RANGE 0 7)
            vellum_gui_test(ui.application_early_allocation_${index} $<TARGET_FILE:vellum_app_allocation_fault_tests>)
            set_tests_properties(ui.application_early_allocation_${index} PROPERTIES TIMEOUT 30
                ENVIRONMENT "VELLUM_TEST_APP_FAILURE=early${index}")
        endforeach()
        foreach(index RANGE 0 7)
            vellum_gui_test(ui.application_open_allocation_${index} $<TARGET_FILE:vellum_app_allocation_fault_tests>
                ${PROJECT_BINARY_DIR}/reference.pdf)
            set_tests_properties(ui.application_open_allocation_${index} PROPERTIES TIMEOUT 30
                ENVIRONMENT "VELLUM_TEST_APP_FAILURE=open${index}")
        endforeach()
        foreach(index RANGE 0 5)
            vellum_gui_test(ui.application_report_allocation_${index} $<TARGET_FILE:vellum_app_allocation_fault_tests>
                ${PROJECT_BINARY_DIR}/reference.pdf)
            set_tests_properties(ui.application_report_allocation_${index} PROPERTIES TIMEOUT 30
                ENVIRONMENT "VELLUM_TEST_APP_FAILURE=report${index}")
        endforeach()
        foreach(index RANGE 0 5)
            vellum_gui_test(ui.application_label_allocation_${index} $<TARGET_FILE:vellum_app_allocation_fault_tests>
                ${PROJECT_BINARY_DIR}/reference.pdf)
            set_tests_properties(ui.application_label_allocation_${index} PROPERTIES TIMEOUT 30
                ENVIRONMENT "VELLUM_TEST_APP_FAILURE=label${index}")
        endforeach()
        vellum_gui_test(ui.application_foreign_exception $<TARGET_FILE:vellum_app_allocation_fault_tests>
            ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.application_foreign_exception PROPERTIES TIMEOUT 30
            ENVIRONMENT "VELLUM_TEST_APP_FAILURE=foreign")
    endif()
    vellum_gui_test(ui.native_file_workflow ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/tests/test_native_file_workflow.py $<TARGET_FILE:vellum>
        ${PROJECT_BINARY_DIR}/reference.pdf)
    set_tests_properties(ui.native_file_workflow PROPERTIES TIMEOUT 120
        ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
    file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/ui-home)
    set_tests_properties(ui.reader_navigation_editor PROPERTIES TIMEOUT 30
        ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
    vellum_test_executable(vellum_print_ui_tests tests/test_print_ui.cc)
    target_link_libraries(vellum_print_ui_tests PRIVATE vellum_printing ${CMAKE_DL_LIBS})
    vellum_gui_test(ui.live_print_preview $<TARGET_FILE:vellum_print_ui_tests> ${PROJECT_BINARY_DIR}/reference.pdf
        ${PROJECT_SOURCE_DIR}/tests/fixtures/restricted.pdf ${PROJECT_BINARY_DIR}/print-preview.png)
    set_tests_properties(ui.live_print_preview PROPERTIES TIMEOUT 120 ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
    include(cmake/TestsGuiFaults.cmake)
    vellum_test_executable(vellum_device_tests tests/test_print_device.cc)
    target_link_libraries(vellum_device_tests PRIVATE vellum_printing)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(vellum_device_tests PRIVATE ${CMAKE_DL_LIBS})
        target_link_options(vellum_device_tests PRIVATE -rdynamic
            "-Wl,--wrap=_ZNK6vellum12pdf_document9may_printEv,--wrap=_ZN17wxPrintDialogDataC1ERKS_,--wrap=_ZN17wxPrintDialogDataaSERKS_,--wrap=_ZN13wxPrintDialogC1EP8wxWindowP17wxPrintDialogData,--wrap=_ZN10wxPrintoutC2ERK8wxString,--wrap=_ZN9wxPrinterC1EP17wxPrintDialogData")
    endif()
    vellum_gui_test(printing.virtual_device $<TARGET_FILE:vellum_device_tests> ${PROJECT_BINARY_DIR}/reference.pdf ${PROJECT_BINARY_DIR}/printed.ps ${PROJECT_SOURCE_DIR}/tests/fixtures)
    set_tests_properties(printing.virtual_device PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(VELLUM_SANITIZER STREQUAL "address")
        vellum_gui_test(printing.virtual_device_leaks $<TARGET_FILE:vellum_device_tests>
            ${PROJECT_BINARY_DIR}/reference.pdf ${PROJECT_BINARY_DIR}/printed-leak.ps ${PROJECT_SOURCE_DIR}/tests/fixtures)
        set_tests_properties(printing.virtual_device_leaks PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1;LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tests/lsan-fontconfig.supp")
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(ui.print_allocation_failures ui.viewer_allocation_failures
        ui.document_open_allocation_failures ui.document_save_allocation_failures
        ui.editor_allocation_failures ui.viewer_print_allocation_failures
        ui.authorization_allocation_failures ui.print_regeneration_allocation_failures
        ui.viewer_zoom_allocation_failures printing.painter_allocation_failures
        PROPERTIES PROCESSORS 4)
    endif()
    vellum_test_executable(vellum_v8_tests tests/test_v8.cc)
    add_dependencies(vellum_v8_tests vellum-script-worker)
    target_link_libraries(vellum_v8_tests PRIVATE vellum_scripting_transport PkgConfig::VELLUM_SYSTEM_ICU)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME v8.isolated_execution COMMAND vellum_v8_tests
            $<TARGET_FILE:vellum-script-worker> $<TARGET_FILE:vellum_worker_v8_fault_tests>)
    else()
        add_test(NAME v8.isolated_execution COMMAND vellum_v8_tests $<TARGET_FILE:vellum-script-worker>)
    endif()
    add_test(NAME v8.worker_protocol COMMAND ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/tests/test_worker_protocol.py $<TARGET_FILE:vellum-script-worker>)
    set_tests_properties(v8.worker_protocol PROPERTIES TIMEOUT 60)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        vellum_test_executable(vellum_worker_v8_fault_tests tests/worker_v8_faults.cc src/script_worker.cc)
        add_dependencies(vellum_v8_tests vellum_worker_v8_fault_tests)
        target_link_libraries(vellum_worker_v8_fault_tests PRIVATE vellum_scripting_runtime vellum_v8)
        target_link_options(vellum_worker_v8_fault_tests PRIVATE
            "-Wl,--wrap=_ZN2v82V810InitializeEi,--wrap=_ZN2v82V818InitializePlatformEPNS_8PlatformE,--wrap=_ZN2v82V87DisposeEv"
            "-Wl,--wrap=_ZN2v811ArrayBuffer9Allocator19NewDefaultAllocatorEv,--wrap=_ZN2v87Isolate3NewERKNS0_12CreateParamsE,--wrap=_ZN2v87Isolate5EnterEv,--wrap=_ZN2v87Isolate7DisposeEv"
            "-Wl,--wrap=_ZN2v87Context3NewEPNS_7IsolateEPNS_22ExtensionConfigurationENS_10MaybeLocalINS_14ObjectTemplateEEENS5_INS_5ValueEEENS_33DeserializeInternalFieldsCallbackEPNS_14MicrotaskQueueENS_30DeserializeContextDataCallbackENS_29DeserializeAPIWrapperCallbackE"
            "-Wl,--wrap=_ZN2v86String11NewFromUtf8EPNS_7IsolateEPKcNS_13NewStringTypeEi,--wrap=_ZN2v86String9Utf8ValueC1EPNS_7IsolateENS_5LocalINS_5ValueEEENS0_12WriteOptionsE"
            "-Wl,--wrap=_ZN2v86Script7CompileENS_5LocalINS_7ContextEEENS1_INS_6StringEEEPNS_12ScriptOriginE,--wrap=_ZN2v86Script3RunENS_5LocalINS_7ContextEEE"
            "-Wl,--wrap=_ZN2v88platform18NewDefaultPlatformEiNS0_15IdleTaskSupportENS0_21InProcessStackDumpingESt10unique_ptrINS_17TracingControllerESt14default_deleteIS4_EENS0_12PriorityModeE")
        add_test(NAME v8.worker_v8_failures COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_worker_v8_faults.py $<TARGET_FILE:vellum_worker_v8_fault_tests>)
        set_tests_properties(v8.worker_v8_failures PROPERTIES TIMEOUT 30)
        if(VELLUM_SANITIZER STREQUAL "address")
            set_tests_properties(v8.worker_v8_failures PROPERTIES ENVIRONMENT "VELLUM_TEST_ASAN=1")
        endif()
    endif()
    if(NOT VELLUM_SANITIZER)
        add_library(vellum_worker_startup_faults SHARED tests/worker_startup_faults.cc)
        add_test(NAME v8.worker_security_setup_faults COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_worker_startup_faults.py
            $<TARGET_FILE:vellum-script-worker> $<TARGET_FILE:vellum_worker_startup_faults>)
        set_tests_properties(v8.worker_security_setup_faults PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    endif()
    set_tests_properties(pdf.render_copy_validate v8.isolated_execution PROPERTIES TIMEOUT 30)
endif()
