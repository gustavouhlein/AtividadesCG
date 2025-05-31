#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>

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
};

std::vector<Mesh> meshes;
int selectedMesh = 0;
const GLuint WIDTH = 800, HEIGHT = 600;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

struct Light {
    glm::vec3 position = glm::vec3(1.2f, 1.0f, 2.0f);
    glm::vec3 ambient = glm::vec3(0.2f);
    glm::vec3 diffuse = glm::vec3(0.8f);
    glm::vec3 specular = glm::vec3(1.0f);
};
Light light;

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
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos_world;
uniform sampler2D textureSampler;

uniform bool useOverrideColor;
uniform vec3 overrideColor;

void main() {
    vec3 final_color;

    vec3 ambient = light.ambient_color * material.Ka;

    vec3 norm = normalize(fragNormal_world);
    vec3 lightDir = normalize(light.position_world - fragPos_world);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse_base_color = material.Kd;
    if (material.hasTexture) {
        diffuse_base_color *= texture(textureSampler, fragTexCoord).rgb;
    }
    vec3 diffuse = light.diffuse_color * diff * diffuse_base_color;

    vec3 viewDir = normalize(viewPos_world - fragPos_world);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec_intensity = pow(max(dot(viewDir, reflectDir), 0.0), material.Ns);
    vec3 specular = light.specular_color * spec_intensity * material.Ks;
    
    final_color = ambient + diffuse + specular;

    if (useOverrideColor) {
        final_color += overrideColor; 
    }
    
    FragColor = vec4(final_color, 1.0);
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
            temp_vertices.push_back(vert * 0.5f);
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
                        cerr << "Aviso: Normal ausente para o vértice, usando padrão. OBJ: " << filePath << " Face str: " << word << endl;
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
                        if (p_slash1 != string::npos && p_slash1 + 1 != p_slash2 && (p_slash2 == string::npos || p_slash2 > p_slash1 +1) ) {
                             cerr << "Aviso: Coordenada de textura ausente/inválida para o vértice, usando padrão. OBJ: " << filePath << " Face str: " << word << endl;
                        }
                    }
                    next_index++;
                }
                indices_data.push_back(vertex_to_index_map[word]);
            }
        }
    }
    file.close();

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

void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (meshes.empty()) return;

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        Mesh& mesh = meshes[selectedMesh];

        if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
            selectedMesh = (selectedMesh + 1) % meshes.size();

        if (key == GLFW_KEY_W) mesh.translation.y += 0.1f;
        if (key == GLFW_KEY_S) mesh.translation.y -= 0.1f;
        if (key == GLFW_KEY_A) mesh.translation.x -= 0.1f;
        if (key == GLFW_KEY_D) mesh.translation.x += 0.1f;
        if (key == GLFW_KEY_Q) mesh.translation.z += 0.1f;
        if (key == GLFW_KEY_E) mesh.translation.z -= 0.1f;

        if (key == GLFW_KEY_X) mesh.rotation.x += glm::radians(5.0f);
        if (key == GLFW_KEY_Y) mesh.rotation.y += glm::radians(5.0f);
        if (key == GLFW_KEY_Z) mesh.rotation.z += glm::radians(5.0f);

        if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) mesh.scale *= 1.1f;
        if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) mesh.scale = std::max(0.1f, mesh.scale * 0.9f);
    
        if (key == GLFW_KEY_UP) light.position.y += 0.1f;
        if (key == GLFW_KEY_DOWN) light.position.y -= 0.1f;
        if (key == GLFW_KEY_LEFT) light.position.x -= 0.1f;
        if (key == GLFW_KEY_RIGHT) light.position.x += 0.1f;
        if (key == GLFW_KEY_PAGE_UP) light.position.z -= 0.1f;
        if (key == GLFW_KEY_PAGE_DOWN) light.position.z += 0.1f;
    }

     if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "OBJ com Phong e Textura", nullptr, nullptr);
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
    
    GLint lightPosLoc = glGetUniformLocation(shaderProgram, "light.position_world");
    GLint lightAmbientLoc = glGetUniformLocation(shaderProgram, "light.ambient_color");
    GLint lightDiffuseLoc = glGetUniformLocation(shaderProgram, "light.diffuse_color");
    GLint lightSpecularLoc = glGetUniformLocation(shaderProgram, "light.specular_color");

    GLint matKaLoc = glGetUniformLocation(shaderProgram, "material.Ka");
    GLint matKdLoc = glGetUniformLocation(shaderProgram, "material.Kd");
    GLint matKsLoc = glGetUniformLocation(shaderProgram, "material.Ks");
    GLint matNsLoc = glGetUniformLocation(shaderProgram, "material.Ns");
    GLint matHasTextureLoc = glGetUniformLocation(shaderProgram, "material.hasTexture");
    GLint textureSamplerLoc = glGetUniformLocation(shaderProgram, "textureSampler");

    GLint useOverrideColorLoc = glGetUniformLocation(shaderProgram, "useOverrideColor");
    GLint overrideColorLoc = glGetUniformLocation(shaderProgram, "overrideColor");

    std::vector<string> paths = {
        "assets/Modelos3D/cube.obj",
        "assets/Modelos3D/Suzanne.obj"
    };
    
    for (const auto& path : paths) {
        Mesh mesh;
        if (loadSimpleOBJ(path, mesh)) {
            mesh.translation = glm::vec3(meshes.size() * 1.5f - (paths.size()-1)*0.75f , 0, 0);
            mesh.rotation = glm::vec3(0.0f, glm::radians(0.0f), 0.0f);
            mesh.scale = 1.0f;
            meshes.push_back(mesh);
        } else {
            cerr << "Falha ao carregar a malha: " << path << endl;
        }
    }
    if (meshes.empty()) {
        cerr << "Nenhuma malha carregada. Certifique-se de que os caminhos estão corretos e os arquivos OBJ/MTL são válidos." << endl;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));

        glUniform3fv(lightPosLoc, 1, glm::value_ptr(light.position));
        glUniform3fv(lightAmbientLoc, 1, glm::value_ptr(light.ambient));
        glUniform3fv(lightDiffuseLoc, 1, glm::value_ptr(light.diffuse));
        glUniform3fv(lightSpecularLoc, 1, glm::value_ptr(light.specular));

        for (int i = 0; i < meshes.size(); ++i) {
            Mesh& mesh = meshes[i];
            
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, mesh.translation);
            model = glm::rotate(model, mesh.rotation.x, glm::vec3(1, 0, 0));
            model = glm::rotate(model, mesh.rotation.y, glm::vec3(0, 1, 0));
            model = glm::rotate(model, mesh.rotation.z, glm::vec3(0, 0, 1));
            model = glm::scale(model, glm::vec3(mesh.scale));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
            glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
            
            glUniform3fv(matKaLoc, 1, glm::value_ptr(mesh.material.Ka));
            glUniform3fv(matKdLoc, 1, glm::value_ptr(mesh.material.Kd));
            glUniform3fv(matKsLoc, 1, glm::value_ptr(mesh.material.Ks));
            glUniform1f(matNsLoc, mesh.material.Ns);
            glUniform1i(matHasTextureLoc, mesh.material.hasTexture);

            if (mesh.material.hasTexture && mesh.textureID != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh.textureID);
                glUniform1i(textureSamplerLoc, 0);
            } else {
                 glBindTexture(GL_TEXTURE_2D, 0);
            }
            
            glUniform1i(useOverrideColorLoc, i == selectedMesh);
            if (i == selectedMesh) {
                 glUniform3f(overrideColorLoc, 0.18f, 0.18f, 0.25f);
            }
            
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.nIndices, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        glfwSwapBuffers(window);
    }

    glDeleteProgram(shaderProgram);
    for (auto& mesh : meshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);
        if (mesh.textureID != 0) {
            glDeleteTextures(1, &mesh.textureID);
        }
    }
    meshes.clear();

    glfwTerminate();
    return 0;
}