

#include "test_graphs.h"

#include "BlockTreeTopologicalSort.h"
#include "BiconnectedComponentsTests.h"
#include "MatrixIncidences.h"


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
#if defined(_WIN32) && !defined(__MINGW32__)
    std::wcout.imbue(std::locale("rus_rus.866"));
#endif
    int res = RUN_ALL_TESTS();
    return res;
}
