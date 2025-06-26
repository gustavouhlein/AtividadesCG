#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <cmath>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

const GLuint WIDTH = 1200, HEIGHT = 900;

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;

    float MovementSpeed;
    float MouseSensitivity;
    float Fov;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f) 
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(2.5f), MouseSensitivity(0.1f), Fov(45.0f) {
        Position = position;
        WorldUp = up;
        Yaw = yaw;
        Pitch = pitch;
        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix() {
        return glm::lookAt(Position, Position + Front, Up);
    }

    void ProcessKeyboard(string direction, float deltaTime) {
        float velocity = MovementSpeed * deltaTime;
        if (direction == "FORWARD")
            Position += Front * velocity;
        if (direction == "BACKWARD")
            Position -= Front * velocity;
        if (direction == "LEFT")
            Position -= Right * velocity;
        if (direction == "RIGHT")
            Position += Right * velocity;
        if (direction == "UP")
            Position += WorldUp * velocity;
        if (direction == "DOWN")
            Position -= WorldUp * velocity;
    }

    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true) {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (constrainPitch) {
            if (Pitch > 89.0f) Pitch = 89.0f;
            if (Pitch < -89.0f) Pitch = -89.0f;
        }

        updateCameraVectors();
    }

    void ProcessMouseScroll(float yoffset) {
        Fov -= (float)yoffset;
        if (Fov < 1.0f) Fov = 1.0f;
        if (Fov > 45.0f) Fov = 45.0f;
    }

private:
    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};

struct TrajectoryPoint {
    glm::vec3 position;
    TrajectoryPoint(glm::vec3 pos) : position(pos) {}
};

struct Trajectory {
    std::vector<TrajectoryPoint> points;
    int currentPointIndex = 0;
    float t = 0.0f;
    float speed = 2.0f;
    bool isActive = false;
    
    void addPoint(glm::vec3 point) {
        points.push_back(TrajectoryPoint(point));
        if (points.size() >= 2) {
            isActive = true;
        }
    }
    
    glm::vec3 getCurrentPosition(float deltaTime) {
        if (points.empty()) return glm::vec3(0.0f);
        if (points.size() == 1) return points[0].position;
        
        int nextIndex = (currentPointIndex + 1) % points.size();
        float segmentDistance = glm::distance(points[currentPointIndex].position, points[nextIndex].position);
        
        float tIncrement = (segmentDistance > 0.0f) ? (speed * deltaTime) / segmentDistance : 1.0f;
        
        t += tIncrement;
        
        if (t >= 1.0f) {
            t = 0.0f;
            currentPointIndex = (currentPointIndex + 1) % points.size();
        }
        
        nextIndex = (currentPointIndex + 1) % points.size();
        return glm::mix(points[currentPointIndex].position, points[nextIndex].position, t);
    }
    
    void clear() {
        points.clear();
        currentPointIndex = 0;
        t = 0.0f;
        isActive = false;
    }
};

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
    float intensity = 1.0f;
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
    Trajectory trajectory;
    string name = "";
    bool isSelected = false;
};

Camera camera(glm::vec3(0.0f, 2.0f, 5.0f));
std::vector<Mesh> meshes;
std::vector<Light> lights;
int selectedMesh = 0;

bool firstMouse = true;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

GLuint pointVAO, pointVBO;
GLuint lineVAO, lineVBO;
GLuint previewPointVAO, previewPointVBO;

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
uniform bool useOverride;
uniform vec3 overrideColor;

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
    float intensity;
};

uniform Material material;
uniform Light lights[8];
uniform int numLights;
uniform vec3 viewPos_world;
uniform sampler2D textureSampler;
uniform bool useOverride;
uniform vec3 overrideColor;

vec3 calculatePhongLighting(Light light, vec3 fragPos, vec3 normal, vec3 viewDir, vec3 materialColor) {
    if (!light.enabled) {
        return vec3(0.0);
    }
    
    vec3 ambient = light.ambient_color * material.Ka * light.intensity;
    
    vec3 lightDir = normalize(light.position_world - fragPos);
    float distance = length(light.position_world - fragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.diffuse_color * diff * materialColor * attenuation * light.intensity;
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec_intensity = pow(max(dot(viewDir, reflectDir), 0.0), material.Ns);
    vec3 specular = light.specular_color * spec_intensity * material.Ks * attenuation * light.intensity;
    
    return ambient + diffuse + specular;
}

void main() {
    vec3 norm = normalize(fragNormal_world);
    vec3 viewDir = normalize(viewPos_world - fragPos_world);
    
    vec3 materialColor = material.Kd;
    if (material.hasTexture) {
        materialColor *= texture(textureSampler, fragTexCoord).rgb;
    }
    
    if (useOverride) {
        materialColor = overrideColor;
    }
    
    vec3 finalColor = vec3(0.0);
    for (int i = 0; i < numLights && i < 8; ++i) {
        finalColor += calculatePhongLighting(lights[i], fragPos_world, norm, viewDir, materialColor);
    }
    
    FragColor = vec4(finalColor, 1.0);
}
)";

const char* simpleVertexShaderSource = R"(
#version 450 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* simpleFragmentShaderSource = R"(
#version 450 core
out vec4 FragColor;

uniform vec3 color;

void main() {
    FragColor = vec4(color, 1.0);
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
            cerr << "Erro de compilacao do shader (" << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << "): " << log << endl;
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

GLuint createSimpleShaderProgram() {
    auto compile = [](GLuint type, const char* src) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, 512, NULL, log);
            cerr << "Erro de compilacao do shader simples (" << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << "): " << log << endl;
        }
        return shader;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, simpleVertexShaderSource);
    GLuint fs = compile(GL_FRAGMENT_SHADER, simpleFragmentShaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, NULL, log);
        cerr << "Erro de linkagem do programa shader simples: " << log << endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

void setupVisualizationBuffers() {
    glGenVertexArrays(1, &pointVAO);
    glGenBuffers(1, &pointVBO);
    glBindVertexArray(pointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &previewPointVAO);
    glGenBuffers(1, &previewPointVBO);
    glBindVertexArray(previewPointVAO);
    glBindBuffer(GL_ARRAY_BUFFER, previewPointVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void renderTrajectoryVisualization(GLuint simpleShaderProgram, const glm::mat4& view, const glm::mat4& projection) {
    if (selectedMesh >= meshes.size()) return;
    
    glUseProgram(simpleShaderProgram);
    
    GLint simpleModelLoc = glGetUniformLocation(simpleShaderProgram, "model");
    GLint simpleViewLoc = glGetUniformLocation(simpleShaderProgram, "view");
    GLint simpleProjLoc = glGetUniformLocation(simpleShaderProgram, "projection");
    GLint colorLoc = glGetUniformLocation(simpleShaderProgram, "color");
    
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(simpleModelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(simpleViewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(simpleProjLoc, 1, GL_FALSE, glm::value_ptr(projection));
    
    glm::vec3 previewPoint = camera.Position + camera.Front * 2.0f;
    glBindBuffer(GL_ARRAY_BUFFER, previewPointVBO);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(GLfloat), glm::value_ptr(previewPoint), GL_DYNAMIC_DRAW);
    
    glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f);
    glPointSize(8.0f);
    glBindVertexArray(previewPointVAO);
    glDrawArrays(GL_POINTS, 0, 1);
    
    Mesh& mesh = meshes[selectedMesh];
    if (!mesh.trajectory.points.empty()) {
        std::vector<GLfloat> pointData;
        for (const auto& point : mesh.trajectory.points) {
            pointData.push_back(point.position.x);
            pointData.push_back(point.position.y);
            pointData.push_back(point.position.z);
        }
        
        glBindBuffer(GL_ARRAY_BUFFER, pointVBO);
        glBufferData(GL_ARRAY_BUFFER, pointData.size() * sizeof(GLfloat), pointData.data(), GL_DYNAMIC_DRAW);
        
        glUniform3f(colorLoc, 0.0f, 1.0f, 0.0f);
        glPointSize(10.0f);
        glBindVertexArray(pointVAO);
        glDrawArrays(GL_POINTS, 0, mesh.trajectory.points.size());
    }
    
    if (mesh.trajectory.points.size() > 1) {
        std::vector<GLfloat> lineData;
        for (size_t i = 0; i < mesh.trajectory.points.size(); ++i) {
            lineData.push_back(mesh.trajectory.points[i].position.x);
            lineData.push_back(mesh.trajectory.points[i].position.y);
            lineData.push_back(mesh.trajectory.points[i].position.z);
            
            size_t nextIndex = (i + 1) % mesh.trajectory.points.size();
            lineData.push_back(mesh.trajectory.points[nextIndex].position.x);
            lineData.push_back(mesh.trajectory.points[nextIndex].position.y);
            lineData.push_back(mesh.trajectory.points[nextIndex].position.z);
        }
        
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, lineData.size() * sizeof(GLfloat), lineData.data(), GL_DYNAMIC_DRAW);
        
        glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f);
        glLineWidth(2.0f);
        glBindVertexArray(lineVAO);
        glDrawArrays(GL_LINES, 0, lineData.size() / 3);
    }
    
    glBindVertexArray(0);
}

string trim(const string& str) {
    size_t first = str.find_first_not_of(' ');
    if (string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

vector<string> split(const string& str, char delimiter) {
    vector<string> tokens;
    stringstream ss(str);
    string token;
    while (getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

bool loadSceneConfig(const string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        cerr << "Erro ao abrir arquivo de configuracao: " << configPath << endl;
        return false;
    }

    string line;
    string currentSection = "";
    
    while (getline(file, line)) {
        line = trim(line);
        
        if (line.empty() || line[0] == '#') continue;
        
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        size_t equalPos = line.find('=');
        if (equalPos == string::npos) continue;
        
        string key = trim(line.substr(0, equalPos));
        string value = trim(line.substr(equalPos + 1));
        
        if (currentSection == "camera") {
            if (key == "position") {
                vector<string> coords = split(value, ',');
                if (coords.size() >= 3) {
                    camera.Position.x = stof(coords[0]);
                    camera.Position.y = stof(coords[1]);
                    camera.Position.z = stof(coords[2]);
                }
            } else if (key == "yaw") {
                camera.Yaw = stof(value);
            } else if (key == "pitch") {
                camera.Pitch = stof(value);
            } else if (key == "fov") {
                camera.Fov = stof(value);
            }
        } else if (currentSection == "lights") {
            static Light currentLight;
            static bool lightInProgress = false;
            
            if (key == "position") {
                vector<string> coords = split(value, ',');
                if (coords.size() >= 3) {
                    currentLight.position.x = stof(coords[0]);
                    currentLight.position.y = stof(coords[1]);
                    currentLight.position.z = stof(coords[2]);
                    lightInProgress = true;
                }
            } else if (key == "ambient") {
                vector<string> coords = split(value, ',');
                if (coords.size() >= 3) {
                    currentLight.ambient.r = stof(coords[0]);
                    currentLight.ambient.g = stof(coords[1]);
                    currentLight.ambient.b = stof(coords[2]);
                }
            } else if (key == "diffuse") {
                vector<string> coords = split(value, ',');
                if (coords.size() >= 3) {
                    currentLight.diffuse.r = stof(coords[0]);
                    currentLight.diffuse.g = stof(coords[1]);
                    currentLight.diffuse.b = stof(coords[2]);
                }
            } else if (key == "specular") {
                vector<string> coords = split(value, ',');
                if (coords.size() >= 3) {
                    currentLight.specular.r = stof(coords[0]);
                    currentLight.specular.g = stof(coords[1]);
                    currentLight.specular.b = stof(coords[2]);
                }
            } else if (key == "intensity") {
                currentLight.intensity = stof(value);
            } else if (key == "enabled") {
                currentLight.enabled = (value == "true" || value == "1");
            } else if (key == "end") {
                if (lightInProgress) {
                    lights.push_back(currentLight);
                    currentLight = Light();
                    lightInProgress = false;
                }
            }
        } else if (currentSection == "objects") {
            static Mesh currentMesh;
            static bool meshInProgress = false;
            
            if (key == "name") {
                currentMesh.name = value;
                meshInProgress = true;
            } else if (key == "file") {
                if (!loadSimpleOBJ(value, currentMesh)) {
                    cerr << "Falha ao carregar objeto: " << value << endl;
                    meshInProgress = false;
                }
            } else if (key == "translation") {
                vector<string> coords = split(value, ',');
                if (coords.size() >= 3) {
                    currentMesh.translation.x = stof(coords[0]);
                    currentMesh.translation.y = stof(coords[1]);
                    currentMesh.translation.z = stof(coords[2]);
                }
            } else if (key == "rotation") {
                vector<string> coords = split(value, ',');
                if (coords.size() >= 3) {
                    currentMesh.rotation.x = glm::radians(stof(coords[0]));
                    currentMesh.rotation.y = glm::radians(stof(coords[1]));
                    currentMesh.rotation.z = glm::radians(stof(coords[2]));
                }
            } else if (key == "scale") {
                currentMesh.scale = stof(value);
            } else if (key == "trajectory_points") {
                vector<string> points = split(value, ';');
                for (const string& pointStr : points) {
                    vector<string> coords = split(pointStr, ',');
                    if (coords.size() >= 3) {
                        glm::vec3 point(stof(coords[0]), stof(coords[1]), stof(coords[2]));
                        currentMesh.trajectory.addPoint(point);
                    }
                }
            } else if (key == "trajectory_speed") {
                currentMesh.trajectory.speed = stof(value);
            } else if (key == "end") {
                if (meshInProgress) {
                    meshes.push_back(currentMesh);
                    cout << "Carregado objeto: " << currentMesh.name << endl;
                    currentMesh = Mesh();
                    meshInProgress = false;
                }
            }
        }
    }

    file.close();
    return true;
}

void createDefaultScene() {
    lights.clear();
    
    Light keyLight;
    keyLight.position = glm::vec3(2.0f, 3.0f, 2.0f);
    keyLight.ambient = glm::vec3(0.1f);
    keyLight.diffuse = glm::vec3(1.0f);
    keyLight.specular = glm::vec3(1.0f);
    keyLight.intensity = 1.0f;
    keyLight.enabled = true;
    lights.push_back(keyLight);
    
    Light fillLight;
    fillLight.position = glm::vec3(-2.0f, 1.0f, 1.0f);
    fillLight.ambient = glm::vec3(0.05f);
    fillLight.diffuse = glm::vec3(0.4f);
    fillLight.specular = glm::vec3(0.3f);
    fillLight.intensity = 0.6f;
    fillLight.enabled = true;
    lights.push_back(fillLight);
    
    Light backLight;
    backLight.position = glm::vec3(0.0f, 2.0f, -3.0f);
    backLight.ambient = glm::vec3(0.02f);
    backLight.diffuse = glm::vec3(0.6f);
    backLight.specular = glm::vec3(0.8f);
    backLight.intensity = 0.4f;
    backLight.enabled = true;
    lights.push_back(backLight);
    
    std::vector<string> defaultPaths = {
        "assets/Modelos3D/Suzanne.obj",
        "assets/Modelos3D/SuzanneSubdiv1.obj",
        "assets/Modelos3D/cube.obj",
    };
    
    meshes.clear();
    for (size_t i = 0; i < defaultPaths.size(); ++i) {
        Mesh mesh;
        if (loadSimpleOBJ(defaultPaths[i], mesh)) {
            mesh.name = "Object" + to_string(i);
            mesh.translation = glm::vec3(i * 2.0f - 3.0f, 0.0f, 0.0f);
            mesh.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            mesh.scale = 1.0f;
            meshes.push_back(mesh);
            cout << "Carregado objeto padrao: " << defaultPaths[i] << endl;
        }
    }
    
    if (meshes.empty()) {
        cout << "Nenhum objeto foi carregado. Certifique-se de que há arquivos OBJ válidos ou um arquivo de configuracao." << endl;
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

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Leitor/Visualizador de Cenas 3D - Final", nullptr, nullptr);
    if (!window) {
        cerr << "Falha ao criar janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    GLuint shaderProgram = createShaderProgram();
    GLuint simpleShaderProgram = createSimpleShaderProgram();
    
    setupVisualizationBuffers();
    
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint normalMatrixLoc = glGetUniformLocation(shaderProgram, "normalMatrix");
    GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos_world");
    GLint numLightsLoc = glGetUniformLocation(shaderProgram, "numLights");
    GLint useOverrideLoc = glGetUniformLocation(shaderProgram, "useOverride");
    GLint overrideColorLoc = glGetUniformLocation(shaderProgram, "overrideColor");
    
    GLint matKaLoc = glGetUniformLocation(shaderProgram, "material.Ka");
    GLint matKdLoc = glGetUniformLocation(shaderProgram, "material.Kd");
    GLint matKsLoc = glGetUniformLocation(shaderProgram, "material.Ks");
    GLint matNsLoc = glGetUniformLocation(shaderProgram, "material.Ns");
    GLint matHasTextureLoc = glGetUniformLocation(shaderProgram, "material.hasTexture");
    GLint textureSamplerLoc = glGetUniformLocation(shaderProgram, "textureSampler");

    std::vector<GLint> lightPosLocs, lightAmbientLocs, lightDiffuseLocs, lightSpecularLocs, lightEnabledLocs, lightIntensityLocs;
    for (int i = 0; i < 8; ++i) {
        string baseName = "lights[" + to_string(i) + "]";
        lightPosLocs.push_back(glGetUniformLocation(shaderProgram, (baseName + ".position_world").c_str()));
        lightAmbientLocs.push_back(glGetUniformLocation(shaderProgram, (baseName + ".ambient_color").c_str()));
        lightDiffuseLocs.push_back(glGetUniformLocation(shaderProgram, (baseName + ".diffuse_color").c_str()));
        lightSpecularLocs.push_back(glGetUniformLocation(shaderProgram, (baseName + ".specular_color").c_str()));
        lightEnabledLocs.push_back(glGetUniformLocation(shaderProgram, (baseName + ".enabled").c_str()));
        lightIntensityLocs.push_back(glGetUniformLocation(shaderProgram, (baseName + ".intensity").c_str()));
    }

    if (!loadSceneConfig("scene_config.txt")) {
        cout << "Arquivo de configuracao nao encontrado, criando cena padrao..." << endl;
        createDefaultScene();
    }
    
    cout << "=== CONTROLES ===" << endl;
    cout << "W/A/S/D: Mover a camera" << endl;
    cout << "ESPACO: Mover camera para cima" << endl;
    cout << "SHIFT: Mover camera para baixo" << endl;
    cout << "MOUSE: Olhar ao redor" << endl;
    cout << "SCROLL: Zoom (FOV)" << endl;
    cout << "TAB: Alternar entre objetos" << endl;
    cout << "=== MANIPULACAO DE OBJETOS ===" << endl;
    cout << "Setas: Mover objeto selecionado (frente/tras/esquerda/direita)" << endl;
    cout << "PAGE UP/DOWN: Mover objeto para cima/baixo" << endl;
    cout << "X/Y/Z: Rotacionar objeto selecionado" << endl;
    cout << "Q/E: Escalar objeto selecionado" << endl;
    cout << "=== TRAJETORIAS ===" << endl;
    cout << "P: Adicionar ponto de trajetoria ao objeto selecionado" << endl;
    cout << "C: Limpar trajetoria do objeto selecionado" << endl;
    cout << "G: Ativar/desativar movimento por trajetoria" << endl;
    cout << "+/-: Aumentar/Diminuir velocidade da trajetoria" << endl;
    cout << "=== OUTROS ===" << endl;
    cout << "1-8: Habilitar/Desabilitar luzes" << endl;
    cout << "ESC: Sair" << endl;
    cout << "=================" << endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        glfwPollEvents();

        for (auto& mesh : meshes) {
            if (mesh.trajectory.isActive && !mesh.trajectory.points.empty()) {
                mesh.translation = mesh.trajectory.getCurrentPosition(deltaTime);
            }
        }

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Fov), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

        glUseProgram(shaderProgram);
        
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(camera.Position));
        glUniform1i(numLightsLoc, min((int)lights.size(), 8));

        for (size_t i = 0; i < min(lights.size(), (size_t)8); ++i) {
            glUniform3fv(lightPosLocs[i], 1, glm::value_ptr(lights[i].position));
            glUniform3fv(lightAmbientLocs[i], 1, glm::value_ptr(lights[i].ambient));
            glUniform3fv(lightDiffuseLocs[i], 1, glm::value_ptr(lights[i].diffuse));
            glUniform3fv(lightSpecularLocs[i], 1, glm::value_ptr(lights[i].specular));
            glUniform1i(lightEnabledLocs[i], lights[i].enabled);
            glUniform1f(lightIntensityLocs[i], lights[i].intensity);
        }

        for (size_t i = 0; i < meshes.size(); ++i) {
            Mesh& mesh = meshes[i];
            
            glUniform3fv(matKaLoc, 1, glm::value_ptr(mesh.material.Ka));
            glUniform3fv(matKdLoc, 1, glm::value_ptr(mesh.material.Kd));
            glUniform3fv(matKsLoc, 1, glm::value_ptr(mesh.material.Ks));
            glUniform1f(matNsLoc, mesh.material.Ns);
            glUniform1i(matHasTextureLoc, mesh.material.hasTexture);

            glUniform1i(useOverrideLoc, mesh.isSelected);
            glUniform3f(overrideColorLoc, 0.8f, 0.8f, 1.0f);

            if (mesh.material.hasTexture && mesh.textureID != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh.textureID);
                glUniform1i(textureSamplerLoc, 0);
            }

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, mesh.translation);
            model = glm::rotate(model, mesh.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, mesh.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, mesh.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
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
        }

        renderTrajectoryVisualization(simpleShaderProgram, view, projection);

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
    glDeleteVertexArrays(1, &pointVAO);
    glDeleteBuffers(1, &pointVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &lineVBO);
    glDeleteVertexArrays(1, &previewPointVAO);
    glDeleteBuffers(1, &previewPointVBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(simpleShaderProgram);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard("FORWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard("BACKWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard("LEFT", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard("RIGHT", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard("UP", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.ProcessKeyboard("DOWN", deltaTime);
        
    if (!meshes.empty() && selectedMesh < meshes.size()) {
        Mesh& mesh = meshes[selectedMesh];
        float moveSpeed = 2.0f * deltaTime;
        float rotSpeed = 90.0f * deltaTime;
        float scaleSpeed = 1.0f * deltaTime;
        
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            mesh.translation.z -= moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            mesh.translation.z += moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            mesh.translation.x -= moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            mesh.translation.x += moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
            mesh.translation.y += moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
            mesh.translation.y -= moveSpeed;
            
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            mesh.scale = max(0.1f, mesh.scale - scaleSpeed);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            mesh.scale += scaleSpeed;
    }
}

void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        switch(key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GL_TRUE);
                break;
                
            case GLFW_KEY_TAB:
                if (!meshes.empty()) {
                    meshes[selectedMesh].isSelected = false;
                    selectedMesh = (selectedMesh + 1) % meshes.size();
                    meshes[selectedMesh].isSelected = true;
                    cout << "Objeto selecionado: " << meshes[selectedMesh].name << endl;
                }
                break;
                
            case GLFW_KEY_P:
                if (!meshes.empty() && selectedMesh < meshes.size()) {
                    glm::vec3 newPoint = camera.Position + camera.Front * 2.0f;
                    meshes[selectedMesh].trajectory.addPoint(newPoint);
                    cout << "Ponto adicionado a trajetoria do objeto: " << meshes[selectedMesh].name << endl;
                }
                break;
                
            case GLFW_KEY_C:
                if (!meshes.empty() && selectedMesh < meshes.size()) {
                    meshes[selectedMesh].trajectory.clear();
                    cout << "Trajetoria limpa para: " << meshes[selectedMesh].name << endl;
                }
                break;
                
            case GLFW_KEY_G:
                if (!meshes.empty() && selectedMesh < meshes.size()) {
                    meshes[selectedMesh].trajectory.isActive = !meshes[selectedMesh].trajectory.isActive;
                    cout << "Movimento por trajetoria " << (meshes[selectedMesh].trajectory.isActive ? "ativado" : "desativado") 
                         << " para: " << meshes[selectedMesh].name << endl;
                }
                break;
                
            case GLFW_KEY_EQUAL:
                if (!meshes.empty() && selectedMesh < meshes.size()) {
                    meshes[selectedMesh].trajectory.speed += 0.5f;
                    cout << "Velocidade da trajetoria: " << meshes[selectedMesh].trajectory.speed << endl;
                }
                break;
                
            case GLFW_KEY_MINUS:
                if (!meshes.empty() && selectedMesh < meshes.size()) {
                    meshes[selectedMesh].trajectory.speed = max(0.5f, meshes[selectedMesh].trajectory.speed - 0.5f);
                    cout << "Velocidade da trajetoria: " << meshes[selectedMesh].trajectory.speed << endl;
                }
                break;
                
            case GLFW_KEY_X:
                if (!meshes.empty() && selectedMesh < meshes.size()) {
                    meshes[selectedMesh].rotation.x += glm::radians(15.0f);
                }
                break;
                
            case GLFW_KEY_Y:
                if (!meshes.empty() && selectedMesh < meshes.size()) {
                    meshes[selectedMesh].rotation.y += glm::radians(15.0f);
                }
                break;
                
            case GLFW_KEY_Z:
                if (!meshes.empty() && selectedMesh < meshes.size()) {
                    meshes[selectedMesh].rotation.z += glm::radians(15.0f);
                }
                break;
                
            case GLFW_KEY_1:
            case GLFW_KEY_2:
            case GLFW_KEY_3:
            case GLFW_KEY_4:
            case GLFW_KEY_5:
            case GLFW_KEY_6:
            case GLFW_KEY_7:
            case GLFW_KEY_8: {
                int lightIndex = key - GLFW_KEY_1;
                if (lightIndex < lights.size()) {
                    lights[lightIndex].enabled = !lights[lightIndex].enabled;
                    cout << "Luz " << (lightIndex + 1) << ": " << (lights[lightIndex].enabled ? "ON" : "OFF") << endl;
                }
                break;
            }
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(yoffset);
}