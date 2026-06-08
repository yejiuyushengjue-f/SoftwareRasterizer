#include <exception>
#include <iostream>

void runAssetLocatorTests();
void runMeshFactoryTests();
void runObjLoaderTests();
void runRendererTests();
void runThreadPoolTests();
void runTestSceneTests();

int main()
{
    try {
        runObjLoaderTests();
        runAssetLocatorTests();
        runMeshFactoryTests();
        runRendererTests();
        runThreadPoolTests();
        runTestSceneTests();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Unknown test failure.\n";
        return 1;
    }

    return 0;
}
