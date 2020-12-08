#include "terminalapplication.h"
#include "ut_defines.h"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <QTest>
#include <QApplication>

QT_BEGIN_NAMESPACE
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
QT_END_NAMESPACE

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    TerminalApplication app(argc, argv);

    QTEST_DISABLE_KEYPAD_NAVIGATION
    QTEST_ADD_GPU_BLACKLIST_SUPPORT

    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();

#ifdef ENABLE_UI_TEST
    QTest::qExec(&app, argc, argv);
#endif

    QTEST_SET_MAIN_SOURCE_PATH

    return ret;
}