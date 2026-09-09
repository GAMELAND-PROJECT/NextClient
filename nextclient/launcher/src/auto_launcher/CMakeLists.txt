add_executable(auto_launcher WIN32 main.cpp ../next_launcher/resources.rc ../next_launcher/app.manifest)
target_link_libraries(auto_launcher PRIVATE ole32 oleaut32 uuid)
set_target_properties(auto_launcher PROPERTIES OUTPUT_NAME Allclient)

add_executable(auto_launcher_tests EXCLUDE_FROM_ALL tests.cpp)
target_link_libraries(auto_launcher_tests PRIVATE ole32 oleaut32 uuid)
