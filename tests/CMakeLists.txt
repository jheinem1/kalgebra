ecm_add_test(sessiontest.cpp
    ../src/sessionplotsmodel.cpp ../src/sessionplotsmodel.h
    ../src/consolemodel.cpp ../src/consolemodel.h
    ../src/exactrational.cpp ../src/exactrational.h
    TEST_NAME sessiontest
    LINK_LIBRARIES Qt::Test KF6::I18n KF6::Analitza KF6::AnalitzaPlot)
target_include_directories(sessiontest PRIVATE ../src)

if(TARGET kalgebra)
    file(GLOB app_sources ../src/*.cpp)
    list(FILTER app_sources EXCLUDE REGEX "/main.cpp$")
    ecm_add_test(apptest.cpp ${app_sources} TEST_NAME apptest)
    get_target_property(app_libraries kalgebra LINK_LIBRARIES)
    target_link_libraries(apptest PRIVATE ${app_libraries} Qt::Test)
    target_include_directories(apptest PRIVATE ../src)
endif()
