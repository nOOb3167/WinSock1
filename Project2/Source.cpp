#include <cstdlib>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

int main() {
    aiLogStream stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT,NULL);
    aiScene *scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej.Nano\\Documents\\BlendTmp\\cube01.dae", aiProcessPreset_TargetRealtime_MaxQuality));

    return EXIT_SUCCESS;
}
