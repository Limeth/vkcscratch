#include <inttypes.h>
#include <string.h>
#include "shader_data.h"

void shaderLoadFile(uint32_t *shaderSize, uint32_t **shaderData, char *shaderPath) {
    /* shaderLoadOld(shaderSize, shaderData); return; */
    FILE *file = fopen(shaderPath, "rb");

    if (file == NULL) {
        fprintf(stderr, "Could not read the shader from `%s`.\n", shaderPath);
        exit(1);
    }

    fseek(file, 0, SEEK_END);

    *shaderSize = ftell(file);

    if ((*shaderSize) % 4 != 0) {
        fprintf(stderr, "Shader size must be a multiple of 4 (in bytes).\n");
        fclose(file);
        exit(1);
    }

    fseek(file, 0, SEEK_SET);

    *shaderData = malloc(*shaderSize);

    if (*shaderData == NULL) {
        fprintf(stderr, "Could not allocate memory for the shader.\n");
        fclose(file);
        exit(1);
    }

    fread(*shaderData, *shaderSize, 1, file);
    fclose(file);
}

void shaderLoadStatic(uint32_t *shaderSize, uint32_t **shaderData) {
    *shaderSize = sizeof(shader);
    *shaderData = (uint32_t*) shader;
}

void shaderLoad(uint32_t *shaderSize, uint32_t **shaderData) {
    shaderLoadStatic(shaderSize, shaderData);
    /* shaderLoadFile(shaderSize, shaderData,  "shader/shader.spv"); */
}
