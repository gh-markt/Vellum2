    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        vellum_test_executable(vellum_print_preview_disk_full_tests tests/test_print_preview_disk_full.cc)
        target_link_libraries(vellum_print_preview_disk_full_tests PRIVATE vellum_printing)
        vellum_gui_test(ui.print_preview_disk_full ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/run_disk_full_test.py $<TARGET_FILE:vellum_print_preview_disk_full_tests>)
        set_tests_properties(ui.print_preview_disk_full PROPERTIES TIMEOUT 120 RESOURCE_LOCK vellum_disk_io
            ENVIRONMENT "VELLUM_DISK_FULL_REFERENCE=${PROJECT_BINARY_DIR}/reference.pdf")
        vellum_test_executable(vellum_print_construction_fault_tests tests/test_print_construction_faults.cc)
        target_link_libraries(vellum_print_construction_fault_tests PRIVATE vellum_printing)
        target_link_options(vellum_print_construction_fault_tests PRIVATE
            "-Wl,--wrap=_ZN12wxStaticTextC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_,--wrap=_ZN8wxButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_,--wrap=_ZN10wxCheckBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_,--wrap=_ZN8wxChoice6CreateEP8wxWindowiRK7wxPointRK6wxSizeiPK8wxStringlRK11wxValidatorRS9_,--wrap=_ZN11wxPanelBase6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString,--wrap=_ZN17wxSpinCtrlGTKBase6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelddddS4_,--wrap=_ZN11wxSizerItemC1EP8wxWindowiiiP8wxObject,--wrap=_ZN11wxSizerItemC1EP7wxSizeriiiP8wxObject,--wrap=_ZN11wxSizerItemC1EiiiiiP8wxObject")
        target_link_options(vellum_print_construction_fault_tests PRIVATE
            "-Wl,--wrap=_ZN15wxFlexGridSizer14AddGrowableColEmi,--wrap=_ZN12wxWindowBase8SetSizerEP7wxSizerb")
        target_link_options(vellum_print_construction_fault_tests PRIVATE
            "-Wl,--wrap=_ZN10wxTextCtrlC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_,--wrap=_ZN8wxString9FromAsciiEPKc")
        target_link_options(vellum_print_construction_fault_tests PRIVATE
            "-Wl,--wrap=_ZN18wxScrollHelperBase13SetScrollRateEii,--wrap=_ZN19wxTopLevelWindowGTK10SetMinSizeERK6wxSize,--wrap=_ZN20wxTopLevelWindowBase6LayoutEv,--wrap=_ZN12wxWindowBase10SetMinSizeERK6wxSize")
        target_link_options(vellum_print_construction_fault_tests PRIVATE
            "-Wl,--wrap=_ZNK6vellum12pdf_document10dimensionsEi,--wrap=_ZNK6vellum12pdf_document5pagesEv")
        target_link_options(vellum_print_construction_fault_tests PRIVATE
            "-Wl,--wrap=_ZN7wxSizer10GetMinSizeEv,--wrap=_ZN8wxWindow18SetBackgroundStyleE17wxBackgroundStyle,--wrap=_ZN16wxSpinCtrlDouble9SetDigitsEj")
        vellum_gui_test(ui.print_construction_faults $<TARGET_FILE:vellum_print_construction_fault_tests>
            ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.print_construction_faults PROPERTIES TIMEOUT 60)
        if(VELLUM_SANITIZER STREQUAL "address")
            vellum_gui_test(ui.print_construction_leaks $<TARGET_FILE:vellum_print_construction_fault_tests>
                ${PROJECT_BINARY_DIR}/reference.pdf)
            set_tests_properties(ui.print_construction_leaks PROPERTIES TIMEOUT 60
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1;LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tests/lsan-fontconfig.supp")
        endif()
        vellum_test_executable(vellum_print_submission_tests tests/test_print_submission.cc)
        target_link_libraries(vellum_print_submission_tests PRIVATE vellum_printing)
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZNK6vellum16printer_settings8selectedEv,--wrap=_ZN6vellum16printer_settings14select_printerEP8wxWindow,--wrap=_ZNK6vellum16printer_settings7measureERKNS_13print_optionsE,--wrap=_ZN6vellum16printer_settings6submitEP8wxWindowRNS_12pdf_documentERKNS_9print_jobEi,--wrap=_Z12wxMessageBoxRK8wxStringS1_lP8wxWindowii,--wrap=_ZN8wxDialog8EndModalEi,--wrap=_ZN6vellum13sheet_painter5paintER4wxDCRKNS_9print_jobEmRK6wxRectib")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_,--wrap=_ZN12wxFileDialog9ShowModalEv")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZNK6vellum16printer_settings4nameB5cxx11Ev")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN10wxMemoryDCC1ER8wxBitmap,--wrap=_ZNK7wxImage8SaveFileER14wxOutputStream12wxBitmapType,--wrap=_ZNK8wxBitmap14ConvertToImageEv,--wrap=_ZN20wxMemoryOutputStreamC1EPvm,--wrap=_ZNK20wxMemoryOutputStream6CopyToEPvm,--wrap=_ZN6vellum12atomic_writeERKNSt10filesystem7__cxx114pathESt17basic_string_viewIcSt11char_traitsIcEERKSt8functionIFvNS_12commit_pointEEE")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN8wxBitmap6CreateEiii,--wrap=_ZN10wxMemoryDC12SelectObjectER8wxBitmap")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN20wxTopLevelWindowBase6LayoutEv")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN7wxTimer5StartEib")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZplPKcRK8wxString,--wrap=_ZN16wxStaticTextBase4WrapEi")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZNK8wxString8ToDoubleEPd,--wrap=_ZNK8wxString6ToLongEPli")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZNK17wxSpinCtrlGTKBase10DoGetValueEv,--wrap=_ZNK17wxSpinCtrlGTKBase8DoGetMinEv,--wrap=_ZNK17wxSpinCtrlGTKBase8DoGetMaxEv")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZNK8wxString6AsCharERK8wxMBConv")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=fprintf,--wrap=_ZN8wxWindow7RefreshEbPK6wxRect,--wrap=_ZN8wxString13DoFormatWcharEPKwz")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN6vellum16printer_settings9set_paperEib,--wrap=_ZN6vellum18select_print_pagesESt17basic_string_viewIcSt11char_traitsIcEEiNS_11page_subsetE,--wrap=_ZN6vellum17compose_print_jobERKSt6vectorINS_11page_extentESaIS1_EERKS0_IiSaIiEERKNS_13print_optionsE")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZNK6vellum12pdf_document5pagesEv")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN6vellum16printer_settings16set_job_defaultsERKNS_13print_optionsE")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZNK12wxFileDialog7GetPathEv")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN16wxWindowDisablerC1Eb")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZNK6vellum12pdf_document9may_printEv")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN9wxPaintDCC2EP8wxWindow")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN5wxLog5OnLogEmRK8wxStringRK15wxLogRecordInfo")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN5wxLog17GetComponentLevelERK8wxString")
        target_link_options(vellum_print_submission_tests PRIVATE
            "-Wl,--wrap=_ZN7wxBrushC1ERK8wxColour12wxBrushStyle")
        vellum_gui_test(ui.print_submission_faults $<TARGET_FILE:vellum_print_submission_tests>
            ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.print_submission_faults PROPERTIES TIMEOUT 30)
        if(VELLUM_SANITIZER STREQUAL "address")
            vellum_gui_test(ui.print_submission_leaks $<TARGET_FILE:vellum_print_submission_tests>
                ${PROJECT_BINARY_DIR}/reference.pdf)
            set_tests_properties(ui.print_submission_leaks PROPERTIES TIMEOUT 60
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1;LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tests/lsan-fontconfig.supp")
        endif()
        vellum_test_executable(vellum_viewer_static_event_tests tests/test_viewer_static_events.cc)
        target_link_libraries(vellum_viewer_static_event_tests PRIVATE vellum_ui)
        target_link_options(vellum_viewer_static_event_tests PRIVATE
            "-Wl,--wrap=_ZN12wxEvtHandler6DoBindEiiiP14wxEventFunctorP8wxObject,--wrap=_ZN8wxDialogC2EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_,--wrap=_ZN15wxFlexGridSizer14AddGrowableColEmi")
        vellum_gui_test(ui.viewer_static_events $<TARGET_FILE:vellum_viewer_static_event_tests>
            ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.viewer_static_events PROPERTIES TIMEOUT 30)
        vellum_test_executable(vellum_viewer_construction_fault_tests tests/test_viewer_construction_faults.cc)
        target_link_libraries(vellum_viewer_construction_fault_tests PRIVATE vellum_ui)
        target_link_options(vellum_viewer_construction_fault_tests PRIVATE
            "-Wl,--wrap=_ZN11wxFrameBaseC2Ev,--wrap=_ZN7wxFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_,--wrap=_ZN7wxTimer4InitEv,--wrap=_ZN11wxPanelBase6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString,--wrap=_ZN18wxScrollHelperBaseC2EP8wxWindow,--wrap=_ZN6vellum10main_frame11build_menusEv,--wrap=_ZN6vellum10main_frame13build_toolbarEv,--wrap=_ZN6vellum10main_frame14update_actionsEv,--wrap=_ZN11wxFrameBase15CreateStatusBarEiliRK8wxString,--wrap=_ZN18wxScrollHelperBase13SetScrollRateEii,--wrap=_ZN14wxStaticBitmapC1EP8wxWindowiRK14wxBitmapBundleRK7wxPointRK6wxSizelRK8wxString,--wrap=_ZN7wxTimer5StartEib,--wrap=_ZN11wxFrameBase13SetStatusTextERK8wxStringi,--wrap=_ZN6wxMenu4InitEv,--wrap=_ZN9wxMenuBarC1Ev,--wrap=_ZN11wxFrameBase10SetMenuBarEP9wxMenuBar,--wrap=_ZN14wxMenuItemBase3NewEP6wxMenuiRK8wxStringS4_10wxItemKindS1_")
        target_link_options(vellum_viewer_construction_fault_tests PRIVATE
            "-Wl,--wrap=_ZN13wxArtProvider9GetBitmapERK8wxStringS2_RK6wxSize,--wrap=_ZN10wxComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeiPS3_lRK11wxValidatorS4_,--wrap=_ZN11wxFrameBase13CreateToolBarEliRK8wxString,--wrap=_ZN8wxString9FromAsciiEPKc,--wrap=_ZN8wxStringC1EPKc")
        foreach(failure frame-base frame-create timer-init-0 timer-init-1 canvas-create scroll-helper menus menu-init-0 menu-init-1 menu-init-2 menu-bar menu-attach toolbar toolbar-unavailable toolbar-zoom toolbar-text-0 toolbar-text-1 toolbar-icon-0 toolbar-icon-1 toolbar-icon-2 toolbar-icon-3 toolbar-icon-4 update status-bar status-unavailable scroll-rate bitmap timer status-text)
            vellum_gui_test(ui.viewer_construction_${failure}_failure $<TARGET_FILE:vellum_viewer_construction_fault_tests> ${failure})
            set_tests_properties(ui.viewer_construction_${failure}_failure PROPERTIES TIMEOUT 30)
        endforeach()
        foreach(position RANGE 0 15)
            vellum_gui_test(ui.viewer_construction_menu-item-${position}_failure $<TARGET_FILE:vellum_viewer_construction_fault_tests> menu-item-${position})
            set_tests_properties(ui.viewer_construction_menu-item-${position}_failure PROPERTIES TIMEOUT 30)
            vellum_gui_test(ui.viewer_construction_menu-text-${position}_failure $<TARGET_FILE:vellum_viewer_construction_fault_tests> menu-text-${position})
            set_tests_properties(ui.viewer_construction_menu-text-${position}_failure PROPERTIES TIMEOUT 30)
        endforeach()
        vellum_test_executable(vellum_viewer_render_fault_tests tests/test_viewer_render_faults.cc)
        target_link_libraries(vellum_viewer_render_fault_tests PRIVATE vellum_ui)
        target_link_options(vellum_viewer_render_fault_tests PRIVATE
            "-Wl,--wrap=_ZNK6vellum12pdf_document10dimensionsEi,--wrap=_ZNK6vellum12pdf_document13render_regionEiNS_14page_rectangleEf,--wrap=_ZN7wxImage6CreateEiib,--wrap=_ZN8wxBitmapC1ERK7wxImageid,--wrap=_ZNK18wxScrollHelperBase22DoCalcScrolledPositionEiiPiS0_,--wrap=_ZN11wxFrameBase13SetStatusTextERK8wxStringi,--wrap=_ZNK18wxScrollHelperBase24DoCalcUnscrolledPositionEiiPiS0_,--wrap=_ZN6vellum19autosave_controller4tickENSt6chrono10time_pointINS1_3_V212steady_clockENS1_8durationIlSt5ratioILl1ELl1000000000EEEEEE")
        target_link_options(vellum_viewer_render_fault_tests PRIVATE
            "-Wl,--wrap=_ZN14wxBitmapBundleC1ERK8wxBitmap,--wrap=_ZN14wxScrollHelper16DoShowScrollbarsE21wxScrollbarVisibilityS0_")
        target_link_options(vellum_viewer_render_fault_tests PRIVATE
            "-Wl,--wrap=_ZN8wxString4TrimEb,--wrap=_ZNK8wxString9CmpNoCaseERKS_,--wrap=_ZNK8wxString8EndsWithERKS_PS_,--wrap=_ZNK8wxString8ToDoubleEPd,--wrap=_ZN8wxStringC1EPKc")
        target_link_options(vellum_viewer_render_fault_tests PRIVATE "-Wl,--wrap=_ZN17wxSpinCtrlGTKBase10DoSetValueEd")
        target_link_options(vellum_viewer_render_fault_tests PRIVATE "-Wl,--wrap=_ZNK6vellum12pdf_document5pagesEv,--wrap=_ZN8wxString13DoFormatWcharEPKwz,--wrap=_ZNSt13runtime_errorC1EPKc")
        foreach(failure dimensions double oversize oversize-height oversize-error-allocation render image image-error-allocation bitmap bitmap-bundle scrollbars scrolled-position status timer-ok timer-error timer-status-error menu-zoom-failure zoom-input-failure page-status-single page-status-double status-count-single status-count-double status-format-single status-format-double outside-horizontal outside-vertical resize-during-layout)
            vellum_gui_test(ui.viewer_render_${failure}_failure $<TARGET_FILE:vellum_viewer_render_fault_tests>
                ${PROJECT_BINARY_DIR}/reference.pdf ${failure})
            set_tests_properties(ui.viewer_render_${failure}_failure PROPERTIES TIMEOUT 30
                ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        endforeach()
        vellum_test_executable(vellum_viewer_file_dialog_fault_tests tests/test_viewer_file_dialog_faults.cc)
        target_link_libraries(vellum_viewer_file_dialog_fault_tests PRIVATE vellum_ui)
        target_link_options(vellum_viewer_file_dialog_fault_tests PRIVATE
            "-Wl,--wrap=_ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_,--wrap=_ZN12wxFileDialog9ShowModalEv,--wrap=_ZN21wxPasswordEntryDialog6CreateEP8wxWindowRK8wxStringS4_S4_lRK7wxPoint,--wrap=_ZN8wxDialog9ShowModalEv,--wrap=_Z12wxMessageBoxRK8wxStringS1_lP8wxWindowii")
        target_link_options(vellum_viewer_file_dialog_fault_tests PRIVATE
            "-Wl,--wrap=_ZN19wxTopLevelWindowGTK8SetTitleERK8wxString,--wrap=_ZNK12wxFileDialog7GetPathEv,--wrap=_ZN17wxSpinCtrlGTKBase10DoSetRangeEdd,--wrap=_ZNK8wxString6AsCharERK8wxMBConv,--wrap=fprintf")
        target_link_options(vellum_viewer_file_dialog_fault_tests PRIVATE
            "-Wl,--wrap=_ZNK6vellum12pdf_document8may_editEv,--wrap=_ZNK6vellum12pdf_document9may_printEv,--wrap=_ZNK6vellum12pdf_document8may_saveEv,--wrap=_ZNK6vellum12pdf_document11may_save_asEv,--wrap=_ZN6vellum12pdf_document18authenticate_ownerESt17basic_string_viewIcSt11char_traitsIcEE,--wrap=_ZplPKcRK8wxString,--wrap=_ZplRK8wxStringPKc")
        target_link_options(vellum_viewer_file_dialog_fault_tests PRIVATE
            "-Wl,--wrap=_ZNK17wxSpinCtrlGTKBase8DoGetMinEv,--wrap=_ZNK17wxSpinCtrlGTKBase8DoGetMaxEv,--wrap=_ZNK17wxSpinCtrlGTKBase10DoGetValueEv,--wrap=_ZN17wxSpinCtrlGTKBase10DoSetValueEd")
        target_link_options(vellum_viewer_file_dialog_fault_tests PRIVATE
            "-Wl,--wrap=_ZplRK8wxStringS1_,--wrap=_ZNK6vellum12pdf_document5pagesEv")
        vellum_gui_test(ui.viewer_file_dialog_failures $<TARGET_FILE:vellum_viewer_file_dialog_fault_tests>
            ${PROJECT_BINARY_DIR}/reference.pdf ${PROJECT_SOURCE_DIR}/tests/fixtures/restricted.pdf
            ${PROJECT_SOURCE_DIR}/tests/fixtures/password.pdf)
        set_tests_properties(ui.viewer_file_dialog_failures PROPERTIES TIMEOUT 30)
        vellum_test_executable(vellum_print_ui_owned_allocation_tests tests/test_print_ui_owned_allocations.cc)
        target_link_libraries(vellum_print_ui_owned_allocation_tests PRIVATE vellum_printing ${CMAKE_DL_LIBS})
        target_link_options(vellum_print_ui_owned_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.print_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_ui_owned_allocations.py
            $<TARGET_FILE:vellum_print_ui_owned_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.print_allocation_failures PROPERTIES TIMEOUT 300
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_gui_test(ui.print_wx_string_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_ui_owned_allocations.py
            $<TARGET_FILE:vellum_print_ui_owned_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf
            "wx string conversion")
        set_tests_properties(ui.print_wx_string_allocation_failures PROPERTIES TIMEOUT 300 PROCESSORS 4
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home;VELLUM_PRINT_ALLOC_SCOPE=wx-string;ASAN_OPTIONS=detect_leaks=0")
        vellum_test_executable(vellum_viewer_owned_allocation_tests tests/test_viewer_owned_allocations.cc)
        target_link_libraries(vellum_viewer_owned_allocation_tests PRIVATE vellum_ui ${CMAKE_DL_LIBS})
        target_link_options(vellum_viewer_owned_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.viewer_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_ui_owned_allocations.py
            $<TARGET_FILE:vellum_viewer_owned_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf Viewer)
        set_tests_properties(ui.viewer_allocation_failures PROPERTIES TIMEOUT 300
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_test_executable(vellum_viewer_open_allocation_tests tests/test_viewer_open_allocations.cc)
        target_link_libraries(vellum_viewer_open_allocation_tests PRIVATE vellum_ui ${CMAKE_DL_LIBS})
        target_link_options(vellum_viewer_open_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.document_open_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_viewer_open_allocations.py
            $<TARGET_FILE:vellum_viewer_open_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.document_open_allocation_failures PROPERTIES TIMEOUT 120
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_test_executable(vellum_viewer_save_allocation_tests tests/test_viewer_save_allocations.cc)
        target_link_libraries(vellum_viewer_save_allocation_tests PRIVATE vellum_ui ${CMAKE_DL_LIBS})
        target_link_options(vellum_viewer_save_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.document_save_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_viewer_save_allocations.py
            $<TARGET_FILE:vellum_viewer_save_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.document_save_allocation_failures PROPERTIES TIMEOUT 120
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_test_executable(vellum_editor_allocation_tests tests/test_editor_allocations.cc)
        target_link_libraries(vellum_editor_allocation_tests PRIVATE vellum_ui ${CMAKE_DL_LIBS})
        target_link_options(vellum_editor_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.editor_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_editor_allocations.py
            $<TARGET_FILE:vellum_editor_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.editor_allocation_failures PROPERTIES TIMEOUT 120)
        vellum_test_executable(vellum_viewer_print_allocation_tests tests/test_viewer_print_allocations.cc)
        target_link_libraries(vellum_viewer_print_allocation_tests PRIVATE vellum_ui ${CMAKE_DL_LIBS})
        target_link_options(vellum_viewer_print_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.viewer_print_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_ui_owned_allocations.py
            $<TARGET_FILE:vellum_viewer_print_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf "Viewer Print")
        set_tests_properties(ui.viewer_print_allocation_failures PROPERTIES TIMEOUT 300
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_test_executable(vellum_authorization_allocation_tests tests/test_authorization_allocations.cc)
        target_link_libraries(vellum_authorization_allocation_tests PRIVATE vellum_ui ${CMAKE_DL_LIBS})
        target_link_options(vellum_authorization_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.authorization_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_ui_owned_allocations.py
            $<TARGET_FILE:vellum_authorization_allocation_tests> ${PROJECT_SOURCE_DIR}/tests/fixtures/long-owner.pdf "Owner authorization")
        set_tests_properties(ui.authorization_allocation_failures PROPERTIES TIMEOUT 300
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_test_executable(vellum_print_regeneration_allocation_tests tests/test_print_regeneration_allocations.cc)
        target_link_libraries(vellum_print_regeneration_allocation_tests PRIVATE vellum_printing ${CMAKE_DL_LIBS})
        target_link_options(vellum_print_regeneration_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.print_regeneration_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_regeneration_allocations.py
            $<TARGET_FILE:vellum_print_regeneration_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(ui.print_regeneration_allocation_failures PROPERTIES TIMEOUT 180
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_gui_test(ui.print_long_regeneration_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_regeneration_allocations.py
            $<TARGET_FILE:vellum_print_regeneration_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf
            "Long-range regeneration")
        set_tests_properties(ui.print_long_regeneration_allocation_failures PROPERTIES TIMEOUT 300 PROCESSORS 4
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home;VELLUM_PRINT_RANGE_STRESS=1")
        vellum_test_executable(vellum_viewer_zoom_allocation_tests tests/test_viewer_zoom_allocations.cc)
        target_link_libraries(vellum_viewer_zoom_allocation_tests PRIVATE vellum_ui ${CMAKE_DL_LIBS})
        target_link_options(vellum_viewer_zoom_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(ui.viewer_zoom_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_regeneration_allocations.py
            $<TARGET_FILE:vellum_viewer_zoom_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf "Viewer zoom")
        set_tests_properties(ui.viewer_zoom_allocation_failures PROPERTIES TIMEOUT 120
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_test_executable(vellum_painter_allocation_tests tests/test_painter_allocation_faults.cc)
        target_link_libraries(vellum_painter_allocation_tests PRIVATE vellum_printing ${CMAKE_DL_LIBS})
        target_link_options(vellum_painter_allocation_tests PRIVATE -rdynamic)
        vellum_gui_test(printing.painter_allocation_failures ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_print_regeneration_allocations.py
            $<TARGET_FILE:vellum_painter_allocation_tests> ${PROJECT_BINARY_DIR}/reference.pdf "Sheet painter")
        set_tests_properties(printing.painter_allocation_failures PROPERTIES TIMEOUT 120
            ENVIRONMENT "HOME=${PROJECT_BINARY_DIR}/ui-home")
        vellum_test_executable(vellum_painter_device_fault_tests tests/test_painter_device_faults.cc)
        target_link_libraries(vellum_painter_device_fault_tests PRIVATE vellum_printing)
        target_link_options(vellum_painter_device_fault_tests PRIVATE
            "-Wl,--wrap=_ZN7wxImage6CreateEiib,--wrap=_ZNK7wxImage8Rotate90Eb,--wrap=_ZNK7wxImage18ConvertToGreyscaleEv,--wrap=_ZN8wxBitmapC1ERK7wxImageid,--wrap=_ZN10wxMemoryDCC1ER8wxBitmap,--wrap=_ZNK6vellum12pdf_document10dimensionsEi,--wrap=_ZNK6vellum12pdf_document19render_print_regionEiNS_14page_rectangleEfi")
        vellum_gui_test(printing.painter_device_failures $<TARGET_FILE:vellum_painter_device_fault_tests>
            ${PROJECT_BINARY_DIR}/reference.pdf)
        set_tests_properties(printing.painter_device_failures PROPERTIES TIMEOUT 60)
        if(VELLUM_SANITIZER STREQUAL "address")
            vellum_gui_test(printing.painter_device_leaks $<TARGET_FILE:vellum_painter_device_fault_tests>
                ${PROJECT_BINARY_DIR}/reference.pdf)
            set_tests_properties(printing.painter_device_leaks PROPERTIES TIMEOUT 60
                ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1;LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tests/lsan-fontconfig.supp")
        endif()
    endif()
