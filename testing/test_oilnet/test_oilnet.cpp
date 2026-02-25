

#include "test_oilnet.h"

#include "TestGraphBuilder.h"
#include "TestHydro.h"
#include "TestMixer.h"
#include "BlockPropagationSolver/BlockPropConfidence.h"
#include "BlockPropagationSolver/BlockPropEndogenous.h"
#include "GraphPropagationSolver/FullPropConfidence.h"
#include "GraphPropagationSolver/FullPropEndogenous.h"
#include "GraphPropagationSolver/FullPropFlow.h"
#include "NodalSolver/TestNodalSolver.h"
#include "NodalSolver/TestNodalSolverBufferBase.h"
#include "NodalSolver/TestNodalSolverRenumbering.h"
#include "Task/CustomPipeSolver.h"
#include "Task/MeasurementsCompleteness.h"
#include "Task/TaskControlsHandling.h"
#include "Serialization/TestGraphParser.h"
#include "Serialization/TestResultSerialization.h"
#include "Task/TaskMeasuresHandling.h"
#include "Task/MeasurementsFuncs.h"
#include "Task/HydroTransportTask.h"
#include "Task/HydroTask.h"
#include "Serialization/TestBoundParserJson.h"
#include "Serialization/TestSettingsParser.h"


#include <filesystem>

std::string prepare_test_folder(const std::string& subfolder)
{
    auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string research_name = std::string(test_info->test_case_name());
    std::string case_name = std::string(test_info->name());

    std::filesystem::path p(__FILE__);
    p = p.parent_path();
    p = p.string() + "_out";
    p = p / (research_name + "." + case_name);
    if (subfolder != "") {
        p = p / subfolder;
    }

    std::filesystem::create_directories(p);

    return p.string() + "\\";
}



std::string get_schemes_folder() {
    std::filesystem::path p(__FILE__);
    p = p.parent_path();
    p = p / ".." / "oiltransport_schemes";

    return p.string() + "/";

}

std::string get_scheme_folder(const std::string& scheme_subdir) {
    return get_schemes_folder() + scheme_subdir;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
#if defined(_WIN32) && !defined(__MINGW32__)
    std::wcout.imbue(std::locale("rus_rus.866"));
#endif
    int res = RUN_ALL_TESTS();
    return res;
}
