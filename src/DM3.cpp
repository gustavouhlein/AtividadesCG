#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// STB_IMAGE for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

struct Mesh {
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    int nIndices;
    glm::vec3 translation;
    glm::vec3 rotation;
    float scale = 1.0f;
    GLuint textureID;
};

std::vector<Mesh> meshes;
int selectedMesh = 0;
const GLuint WIDTH = 800, HEIGHT = 600;

const char* vertexShaderSource = R"(
#version 450 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec2 texCoord;
uniform mat4 model;
uniform bool useOverride;
uniform vec3 overrideColor;
out vec4 fragColor;
out vec2 fragTexCoord;
void main() {
    gl_Position = model * vec4(position, 1.0);
    vec3 finalColor = useOverride ? overrideColor : color;
    fragColor = vec4(finalColor, 1.0);
    fragTexCoord = texCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 450 core
in vec4 fragColor;
in vec2 fragTexCoord;
out vec4 color;
uniform sampler2D textureSampler;
void main() {
    vec4 texColor = texture(textureSampler, fragTexCoord);
    color = fragColor * texColor; // Combine texture with vertex color
}
)";

GLuint loadTexture(const string& texturePath) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        cerr << "Failed to load texture: " << texturePath << endl;
    }
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

string loadMTL(const string& mtlPath) {
    std::ifstream file(mtlPath);
    if (!file.is_open()) {
        cerr << "Erro ao abrir " << mtlPath << endl;
        return "";
    }

    string line, textureFile;
    while (getline(file, line)) {
        istringstream ss(line);
        string word;
        ss >> word;
        if (word == "map_Kd") {
            ss >> textureFile;
            size_t lastSlash = mtlPath.find_last_of("\\/");
            string basePath = (lastSlash == string::npos) ? "" : mtlPath.substr(0, lastSlash + 1);
            textureFile = basePath + textureFile;
            break;
        }
    }
    file.close();
    return textureFile;
}

int loadSimpleOBJ(string filePath, int& nIndices, GLuint& textureID) {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<GLuint> indices;
    std::vector<GLfloat> vBuffer;
    glm::vec3 color = glm::vec3(0.5f, 0.5f, 0.5f);

    string mtlPath = filePath.substr(0, filePath.find_last_of('.')) + ".mtl";
    string texturePath = loadMTL(mtlPath);
    if (!texturePath.empty()) {
        textureID = loadTexture(texturePath);
    } else {
        textureID = 0;
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        cerr << "Erro ao abrir " << filePath << endl;
        return -1;
    }

    std::string line;
    while (getline(file, line)) {
        std::istringstream ss(line);
        std::string word;
        ss >> word;
        if (word == "v") {
            glm::vec3 vert;
            ss >> vert.x >> vert.y >> vert.z;
            vertices.push_back(vert * 0.2f);
        } else if (word == "vt") {
            glm::vec2 tex;
            ss >> tex.x >> tex.y;
            texCoords.push_back(tex);
        } else if (word == "f") {
            for (int i = 0; i < 3; ++i) {
                ss >> word;
                size_t firstSlash = word.find('/');
                size_t secondSlash = word.find('/', firstSlash + 1);
                int vIndex = stoi(word.substr(0, firstSlash)) - 1;
                int vtIndex = (firstSlash == string::npos || secondSlash == firstSlash + 1) ? -1 : stoi(word.substr(firstSlash + 1, secondSlash - firstSlash - 1)) - 1;

                vBuffer.push_back(vertices[vIndex].x);
                vBuffer.push_back(vertices[vIndex].y);
                vBuffer.push_back(vertices[vIndex].z);
                vBuffer.push_back(color.r);
                vBuffer.push_back(color.g);
                vBuffer.push_back(color.b);
                if (vtIndex >= 0 && vtIndex < texCoords.size()) {
                    vBuffer.push_back(texCoords[vtIndex].x);
                    vBuffer.push_back(texCoords[vtIndex].y);
                } else {
                    vBuffer.push_back(0.0f);
                    vBuffer.push_back(0.0f);
                }
                indices.push_back(indices.size());
            }
        }
    }
    file.close();

    GLuint VBO, VAO, EBO;
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    nIndices = indices.size();
    return VAO;
}

GLuint createShaderProgram() {
    auto compile = [](GLuint type, const char* src) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, 512, NULL, log);
            cerr << "Shader error: " << log << endl;
        }
        return shader;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        Mesh& mesh = meshes[selectedMesh];

        if (key == GLFW_KEY_TAB)
            selectedMesh = (selectedMesh + 1) % meshes.size();

        if (key == GLFW_KEY_W) mesh.translation.y += 0.1f;
        if (key == GLFW_KEY_S) mesh.translation.y -= 0.1f;
        if (key == GLFW_KEY_A) mesh.translation.x -= 0.1f;
        if (key == GLFW_KEY_D) mesh.translation.x += 0.1f;

        if (key == GLFW_KEY_X) mesh.rotation.x += 0.1f;
        if (key == GLFW_KEY_Y) mesh.rotation.y += 0.1f;
        if (key == GLFW_KEY_Z) mesh.rotation.z += 0.1f;

        if (key == GLFW_KEY_EQUAL) mesh.scale *= 1.1f;
        if (key == GLFW_KEY_MINUS) mesh.scale = std::max(0.1f, mesh.scale * 0.9f);
    }
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Múltiplos OBJ com Textura", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetKeyCallback(window, key_callback);
    glEnable(GL_DEPTH_TEST);

    GLuint shader = createShaderProgram();
    glUseProgram(shader);

    GLint modelLoc = glGetUniformLocation(shader, "model");
    GLint useOverrideLoc = glGetUniformLocation(shader, "useOverride");
    GLint overrideColorLoc = glGetUniformLocation(shader, "overrideColor");
    GLint textureLoc = glGetUniformLocation(shader, "textureSampler");

    std::vector<string> paths = {
        "assets\\Modelos3D\\Suzanne.obj",
        "assets\\Modelos3D\\SuzanneSubdiv1.obj"
    };

    for (const auto& path : paths) {
        Mesh mesh;
        mesh.VAO = loadSimpleOBJ(path, mesh.nIndices, mesh.textureID);
        mesh.translation = glm::vec3(0, 0, 0);
        mesh.rotation = glm::vec3(0.0f, glm::radians(180.0f), 0.0f);
        mesh.scale = 1.0f;
        meshes.push_back(mesh);
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < meshes.size(); ++i) {
            Mesh& mesh = meshes[i];
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, mesh.translation);
            model = glm::rotate(model, mesh.rotation.x, glm::vec3(1, 0, 0));
            model = glm::rotate(model, mesh.rotation.y, glm::vec3(0, 1, 0));
            model = glm::rotate(model, mesh.rotation.z, glm::vec3(0, 0, 1));
            model = glm::scale(model, glm::vec3(mesh.scale));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            glUniform1i(useOverrideLoc, i == selectedMesh);
            glUniform3f(overrideColorLoc, 0.7f, 0.7f, 1.0f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.textureID);
            glUniform1i(textureLoc, 0);

            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.nIndices, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
    }

    for (auto& mesh : meshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);
        if (mesh.textureID != 0) {
            glDeleteTextures(1, &mesh.textureID);
        }
    }
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}