// Paths of the data and of the working files: "work/..." goes to work_root(), which is work/ in the data
// root or, when that is not writable, a folder of the user (here: a data root in a temporary folder with
// a file named "work" in the way stands for it).
#include "test_util.h"
#include "core/util.h"

#include <string>

static void test_repo(void)
{
    CHECK(data_root_valid());
    CHECK(work_root() == path_join(data_root(), "work"));
    CHECK(data_path("work/test/x.bin") == path_join(work_root(), "test/x.bin"));
    CHECK(data_path("work") == work_root());
    CHECK(data_path("workshop/a") == path_join(data_root(), "workshop/a"));   // only the folder work
    CHECK(data_relative(data_path("work/ide/sapi_hdd.img")) == "work/ide/sapi_hdd.img");
    CHECK(data_relative(data_path("roms/README.md")) == "roms/README.md");
}

static void test_fallback(void)
{
    std::string repo = data_root();
    std::string tmp = path_join(repo, "work/test/paths_root");
    CHECK(make_dirs(path_join(tmp, "roms")));
    CHECK(make_dirs(path_join(tmp, "machines")));
    // "work" is a file: no folder work/ can be made in this data root
    std::string block = path_join(tmp, "work");
    if (!file_exists(block)) CHECK(file_write_text(block, "not a folder"));
    // the user's folder in the temporary folder too
    std::string user = path_join(tmp, "user");
#ifdef _WIN32
    _putenv_s("LOCALAPPDATA", user.c_str());   // ASCII path in the tests
#else
    setenv("XDG_DATA_HOME", user.c_str(), 1);
#endif
    data_root_set(tmp);
    CHECK(data_root_valid());
    std::string w = work_root();
    fprintf(stderr, "fallback work root: %s\n", w.c_str());
    CHECK(w == path_join(path_join(user, "SAPIemu"), "work"));
    CHECK(dir_exists(w));
    CHECK(data_path("work/kzd-1/kazeta.kzd") == path_join(w, "kzd-1/kazeta.kzd"));
    CHECK(data_relative(path_join(w, "kzd-1/kazeta.kzd")) == "work/kzd-1/kazeta.kzd");
    data_root_set(repo);
    CHECK(work_root() == path_join(repo, "work"));
}

int main(void)
{
    test_init();
    test_repo();
    test_fallback();
    return TEST_RESULT();
}
