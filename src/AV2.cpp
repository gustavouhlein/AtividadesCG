#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <cmath>

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

struct Material {
    glm::vec3 Ka = glm::vec3(0.1f);
    glm::vec3 Kd = glm::vec3(0.7f);
    glm::vec3 Ks = glm::vec3(0.2f);
    float Ns = 32.0f;
    string map_Kd_path = "";
    bool hasTexture = false;
};

struct Light {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 ambient = glm::vec3(0.2f);
    glm::vec3 diffuse = glm::vec3(0.8f);
    glm::vec3 specular = glm::vec3(1.0f);
    bool enabled = true;
};

struct Mesh {
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    int nIndices = 0;
    glm::vec3 translation = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    float scale = 1.0f;
    GLuint textureID = 0;
    Material material;
    glm::vec3 boundingBoxMin = glm::vec3(0.0f);
    glm::vec3 boundingBoxMax = glm::vec3(0.0f);
};

Mesh mesh;
const GLuint WIDTH = 800, HEIGHT = 600;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

Light keyLight;    // Luz principal
Light fillLight;   // Luz de preenchimento
Light backLight;   // Luz de fundo

const char* vertexShaderSource = R"(
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 fragPos_world;
out vec3 fragNormal_world;
out vec2 fragTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    fragPos_world = vec3(model * vec4(aPos, 1.0));
    fragNormal_world = normalize(normalMatrix * aNormal);
    fragTexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 450 core
in vec3 fragPos_world;
in vec3 fragNormal_world;
in vec2 fragTexCoord;

out vec4 FragColor;

struct Material {
    vec3 Ka;
    vec3 Kd;
    vec3 Ks;
    float Ns;
    bool hasTexture;
};

struct Light {
    vec3 position_world;
    vec3 ambient_color;
    vec3 diffuse_color;
    vec3 specular_color;
    bool enabled;
};

uniform Material material;
uniform Light keyLight;
uniform Light fillLight;
uniform Light backLight;
uniform vec3 viewPos_world;
uniform sampler2D textureSampler;

vec3 calculatePhongLighting(Light light, vec3 fragPos, vec3 normal, vec3 viewDir, vec3 materialColor) {
    if (!light.enabled) {
        return vec3(0.0);
    }
    
    // Ambient
    vec3 ambient = light.ambient_color * material.Ka;
    
    // Diffuse with attenuation
    vec3 lightDir = normalize(light.position_world - fragPos);
    float distance = length(light.position_world - fragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse_color * diff * materialColor * attenuation;
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec_intensity = pow(max(dot(viewDir, reflectDir), 0.0), material.Ns);
    vec3 specular = light.specular_color * spec_intensity * material.Ks * attenuation;
    
    return ambient + diffuse + specular;
}

void main() {
    vec3 norm = normalize(fragNormal_world);
    vec3 viewDir = normalize(viewPos_world - fragPos_world);
    
    vec3 materialColor = material.Kd;
    if (material.hasTexture) {
        materialColor *= texture(textureSampler, fragTexCoord).rgb;
    }
    
    vec3 keyLighting = calculatePhongLighting(keyLight, fragPos_world, norm, viewDir, materialColor);
    vec3 fillLighting = calculatePhongLighting(fillLight, fragPos_world, norm, viewDir, materialColor);
    vec3 backLighting = calculatePhongLighting(backLight, fragPos_world, norm, viewDir, materialColor);
    
    vec3 finalColor = keyLighting + fillLighting + backLighting;
    
    FragColor = vec4(finalColor, 1.0);
}
)";

GLuint loadTexture(const string& texturePath) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrChannels == 1) format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else if (nrChannels == 4) format = GL_RGBA;
        
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        cerr << "Falha ao carregar textura: " << texturePath << endl;
        textureID = 0;
    }
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

Material loadMTL(const string& mtlPath) {
    Material material;
    std::ifstream file(mtlPath);
    if (!file.is_open()) {
        cerr << "Erro ao abrir MTL: " << mtlPath << endl;
        return material;
    }

    string line;
    while (getline(file, line)) {
        istringstream ss(line);
        string word;
        ss >> word;

        if (word == "Ka") {
            ss >> material.Ka.r >> material.Ka.g >> material.Ka.b;
        } else if (word == "Kd") {
            ss >> material.Kd.r >> material.Kd.g >> material.Kd.b;
        } else if (word == "Ks") {
            ss >> material.Ks.r >> material.Ks.g >> material.Ks.b;
        } else if (word == "Ns") {
            ss >> material.Ns;
        } else if (word == "map_Kd") {
            string textureFile;
            ss >> textureFile;
            size_t lastSlash = mtlPath.find_last_of("\\/");
            string basePath = (lastSlash == string::npos) ? "" : mtlPath.substr(0, lastSlash + 1);
            material.map_Kd_path = basePath + textureFile;
            material.hasTexture = true;
        }
    }
    file.close();
    return material;
}

bool loadSimpleOBJ(const string& filePath, Mesh& outMesh) {
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_texCoords;
    std::vector<glm::vec3> temp_normals;

    std::vector<GLfloat> vBuffer_data;
    std::vector<GLuint> indices_data;
    std::map<string, GLuint> vertex_to_index_map;
    GLuint next_index = 0;

    string mtlFilePath = "";
    
    glm::vec3 minBounds = glm::vec3(FLT_MAX);
    glm::vec3 maxBounds = glm::vec3(-FLT_MAX);
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        cerr << "Erro ao abrir OBJ: " << filePath << endl;
        return false;
    }

    std::string line;
    while (getline(file, line)) {
        std::istringstream ss(line);
        std::string word;
        ss >> word;

        if (word == "mtllib") {
            ss >> mtlFilePath;
            size_t lastSlash = filePath.find_last_of("\\/");
            string basePath = (lastSlash == string::npos) ? "" : filePath.substr(0, lastSlash + 1);
            mtlFilePath = basePath + mtlFilePath;
        } else if (word == "v") {
            glm::vec3 vert;
            ss >> vert.x >> vert.y >> vert.z;
            vert *= 0.5f;
            temp_vertices.push_back(vert);
            
            minBounds = glm::min(minBounds, vert);
            maxBounds = glm::max(maxBounds, vert);
        } else if (word == "vt") {
            glm::vec2 tex;
            ss >> tex.x >> tex.y;
            temp_texCoords.push_back(tex);
        } else if (word == "vn") {
            glm::vec3 norm;
            ss >> norm.x >> norm.y >> norm.z;
            temp_normals.push_back(norm);
        } else if (word == "f") {
            for (int i = 0; i < 3; ++i) {
                ss >> word;
                
                if (vertex_to_index_map.find(word) == vertex_to_index_map.end()) {
                    vertex_to_index_map[word] = next_index;

                    size_t p_slash1 = word.find('/');
                    size_t p_slash2 = word.find('/', p_slash1 + 1);

                    int vIndex = stoi(word.substr(0, p_slash1)) - 1;
                    
                    vBuffer_data.push_back(temp_vertices[vIndex].x);
                    vBuffer_data.push_back(temp_vertices[vIndex].y);
                    vBuffer_data.push_back(temp_vertices[vIndex].z);
                    
                    int vnIndex = -1;
                    if (p_slash2 != string::npos && p_slash2 + 1 < word.length()){
                        vnIndex = stoi(word.substr(p_slash2 + 1)) - 1;
                    }

                    if (vnIndex >= 0 && vnIndex < temp_normals.size()) {
                        vBuffer_data.push_back(temp_normals[vnIndex].x);
                        vBuffer_data.push_back(temp_normals[vnIndex].y);
                        vBuffer_data.push_back(temp_normals[vnIndex].z);
                    } else {
                        vBuffer_data.push_back(0.0f); vBuffer_data.push_back(1.0f); vBuffer_data.push_back(0.0f);
                    }

                    int vtIndex = -1;
                    if (p_slash1 != string::npos && p_slash1 + 1 < word.length() && (p_slash2 == string::npos || p_slash1 + 1 != p_slash2) ) {
                         if (p_slash2 != string::npos) {
                            vtIndex = stoi(word.substr(p_slash1 + 1, p_slash2 - (p_slash1 + 1))) - 1;
                         } else {
                            vtIndex = stoi(word.substr(p_slash1 + 1)) - 1;
                         }
                    }
                    
                    if (vtIndex >= 0 && vtIndex < temp_texCoords.size()) {
                        vBuffer_data.push_back(temp_texCoords[vtIndex].x);
                        vBuffer_data.push_back(temp_texCoords[vtIndex].y);
                    } else {
                        vBuffer_data.push_back(0.0f); vBuffer_data.push_back(0.0f);
                    }
                    next_index++;
                }
                indices_data.push_back(vertex_to_index_map[word]);
            }
        }
    }
    file.close();
    
    outMesh.boundingBoxMin = minBounds;
    outMesh.boundingBoxMax = maxBounds;

    if (!mtlFilePath.empty()) {
        outMesh.material = loadMTL(mtlFilePath);
        if (outMesh.material.hasTexture && !outMesh.material.map_Kd_path.empty()) {
            outMesh.textureID = loadTexture(outMesh.material.map_Kd_path);
            if (outMesh.textureID == 0) {
                outMesh.material.hasTexture = false;
            }
        } else {
             outMesh.material.hasTexture = false;
        }
    } else {
        outMesh.material = Material(); 
        outMesh.material.hasTexture = false;
    }

    glGenVertexArrays(1, &outMesh.VAO);
    glGenBuffers(1, &outMesh.VBO);
    glGenBuffers(1, &outMesh.EBO);

    glBindVertexArray(outMesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, outMesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer_data.size() * sizeof(GLfloat), vBuffer_data.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outMesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_data.size() * sizeof(GLuint), indices_data.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    outMesh.nIndices = indices_data.size();
    return true;
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
            cerr << "Erro de compilação do shader (" << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << "): " << log << endl;
        }
        return shader;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, NULL, log);
        cerr << "Erro de linkagem do programa shader: " << log << endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

void setupThreePointLighting() {
    glm::vec3 objectCenter = mesh.translation;
    glm::vec3 objectSize = mesh.boundingBoxMax - mesh.boundingBoxMin;
    float objectRadius = glm::length(objectSize) * mesh.scale * 0.5f;
    
    // Key Light (Luz Principal) - posição frontal superior à direita
    keyLight.position = objectCenter + glm::vec3(2.0f, 1.5f, 2.0f) * objectRadius;
    keyLight.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    keyLight.diffuse = glm::vec3(1.0f, 1.0f, 1.0f);    // Luz mais intensa
    keyLight.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    keyLight.enabled = true;
    
    // Fill Light (Luz de Preenchimento) - posição frontal à esquerda
    fillLight.position = objectCenter + glm::vec3(-1.5f, 0.5f, 1.5f) * objectRadius;
    fillLight.ambient = glm::vec3(0.05f, 0.05f, 0.05f);
    fillLight.diffuse = glm::vec3(0.4f, 0.4f, 0.4f);   // Menos intensa que a principal
    fillLight.specular = glm::vec3(0.3f, 0.3f, 0.3f);
    fillLight.enabled = true;
    
    // Back Light (Luz de Fundo) - posição traseira superior
    backLight.position = objectCenter + glm::vec3(0.0f, 1.0f, -2.5f) * objectRadius;
    backLight.ambient = glm::vec3(0.02f, 0.02f, 0.02f);
    backLight.diffuse = glm::vec3(0.6f, 0.6f, 0.6f);   // Intensidade média
    backLight.specular = glm::vec3(0.8f, 0.8f, 0.8f);
    backLight.enabled = true;
}

void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        switch(key) {
            case GLFW_KEY_1:
                keyLight.enabled = !keyLight.enabled;
                cout << "Key Light: " << (keyLight.enabled ? "ON" : "OFF") << endl;
                break;
            case GLFW_KEY_2:
                fillLight.enabled = !fillLight.enabled;
                cout << "Fill Light: " << (fillLight.enabled ? "ON" : "OFF") << endl;
                break;
            case GLFW_KEY_3:
                backLight.enabled = !backLight.enabled;
                cout << "Back Light: " << (backLight.enabled ? "ON" : "OFF") << endl;
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GL_TRUE);
                break;
        }
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Iluminacao 3 Pontos", nullptr, nullptr);
    if (!window) {
        cerr << "Falha ao criar janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    glfwSetKeyCallback(window, key_callback);
    glEnable(GL_DEPTH_TEST);

    GLuint shaderProgram = createShaderProgram();
    
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint normalMatrixLoc = glGetUniformLocation(shaderProgram, "normalMatrix");
    GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos_world");
    
    GLint keyLightPosLoc = glGetUniformLocation(shaderProgram, "keyLight.position_world");
    GLint keyLightAmbientLoc = glGetUniformLocation(shaderProgram, "keyLight.ambient_color");
    GLint keyLightDiffuseLoc = glGetUniformLocation(shaderProgram, "keyLight.diffuse_color");
    GLint keyLightSpecularLoc = glGetUniformLocation(shaderProgram, "keyLight.specular_color");
    GLint keyLightEnabledLoc = glGetUniformLocation(shaderProgram, "keyLight.enabled");
    
    GLint fillLightPosLoc = glGetUniformLocation(shaderProgram, "fillLight.position_world");
    GLint fillLightAmbientLoc = glGetUniformLocation(shaderProgram, "fillLight.ambient_color");
    GLint fillLightDiffuseLoc = glGetUniformLocation(shaderProgram, "fillLight.diffuse_color");
    GLint fillLightSpecularLoc = glGetUniformLocation(shaderProgram, "fillLight.specular_color");
    GLint fillLightEnabledLoc = glGetUniformLocation(shaderProgram, "fillLight.enabled");
    
    GLint backLightPosLoc = glGetUniformLocation(shaderProgram, "backLight.position_world");
    GLint backLightAmbientLoc = glGetUniformLocation(shaderProgram, "backLight.ambient_color");
    GLint backLightDiffuseLoc = glGetUniformLocation(shaderProgram, "backLight.diffuse_color");
    GLint backLightSpecularLoc = glGetUniformLocation(shaderProgram, "backLight.specular_color");
    GLint backLightEnabledLoc = glGetUniformLocation(shaderProgram, "backLight.enabled");

    GLint matKaLoc = glGetUniformLocation(shaderProgram, "material.Ka");
    GLint matKdLoc = glGetUniformLocation(shaderProgram, "material.Kd");
    GLint matKsLoc = glGetUniformLocation(shaderProgram, "material.Ks");
    GLint matNsLoc = glGetUniformLocation(shaderProgram, "material.Ns");
    GLint matHasTextureLoc = glGetUniformLocation(shaderProgram, "material.hasTexture");
    GLint textureSamplerLoc = glGetUniformLocation(shaderProgram, "textureSampler");

    if (!loadSimpleOBJ("assets/Modelos3D/Suzanne.obj", mesh)) {
        if (!loadSimpleOBJ("Suzanne.obj", mesh)) {
            cerr << "Falha ao carregar Suzanne.obj." << endl;
            return -1;
        }
    }
    
    mesh.translation = glm::vec3(0.0f, 0.0f, 0.0f);
    mesh.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    mesh.scale = 1.0f;
    
    setupThreePointLighting();
    
    cout << "=== CONTROLES ===" << endl;
    cout << "Tecla 1: Habilitar/Desabilitar Key Light (Luz Principal)" << endl;
    cout << "Tecla 2: Habilitar/Desabilitar Fill Light (Luz de Preenchimento)" << endl;
    cout << "Tecla 3: Habilitar/Desabilitar Back Light (Luz de Fundo)" << endl;
    cout << "ESC: Sair" << endl;
    cout << "=================" << endl;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));

        glUniform3fv(keyLightPosLoc, 1, glm::value_ptr(keyLight.position));
        glUniform3fv(keyLightAmbientLoc, 1, glm::value_ptr(keyLight.ambient));
        glUniform3fv(keyLightDiffuseLoc, 1, glm::value_ptr(keyLight.diffuse));
        glUniform3fv(keyLightSpecularLoc, 1, glm::value_ptr(keyLight.specular));
        glUniform1i(keyLightEnabledLoc, keyLight.enabled);
        
        glUniform3fv(fillLightPosLoc, 1, glm::value_ptr(fillLight.position));
        glUniform3fv(fillLightAmbientLoc, 1, glm::value_ptr(fillLight.ambient));
        glUniform3fv(fillLightDiffuseLoc, 1, glm::value_ptr(fillLight.diffuse));
        glUniform3fv(fillLightSpecularLoc, 1, glm::value_ptr(fillLight.specular));
        glUniform1i(fillLightEnabledLoc, fillLight.enabled);
        
        glUniform3fv(backLightPosLoc, 1, glm::value_ptr(backLight.position));
        glUniform3fv(backLightAmbientLoc, 1, glm::value_ptr(backLight.ambient));
        glUniform3fv(backLightDiffuseLoc, 1, glm::value_ptr(backLight.diffuse));
        glUniform3fv(backLightSpecularLoc, 1, glm::value_ptr(backLight.specular));
        glUniform1i(backLightEnabledLoc, backLight.enabled);

        glUniform3fv(matKaLoc, 1, glm::value_ptr(mesh.material.Ka));
        glUniform3fv(matKdLoc, 1, glm::value_ptr(mesh.material.Kd));
        glUniform3fv(matKsLoc, 1, glm::value_ptr(mesh.material.Ks));
        glUniform1f(matNsLoc, mesh.material.Ns);
        glUniform1i(matHasTextureLoc, mesh.material.hasTexture);

        if (mesh.material.hasTexture && mesh.textureID != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.textureID);
            glUniform1i(textureSamplerLoc, 0);
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, mesh.translation);
        model = glm::rotate(model, glm::radians(mesh.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(mesh.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(mesh.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(mesh.scale));

        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.nIndices, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        if (mesh.material.hasTexture && mesh.textureID != 0) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &mesh.VAO);
    glDeleteBuffers(1, &mesh.VBO);
    glDeleteBuffers(1, &mesh.EBO);
    if (mesh.textureID != 0) {
        glDeleteTextures(1, &mesh.textureID);
    }
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}