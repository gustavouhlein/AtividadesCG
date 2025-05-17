#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Mesh {
    GLuint VAO;
    int nVertices;
    glm::vec3 translation;
    glm::vec3 rotation;
    float scale = 1.0f;
};

std::vector<Mesh> meshes;
int selectedMesh = 0;

const GLuint WIDTH = 800, HEIGHT = 600;

const char* vertexShaderSource = "#version 450 core\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"uniform mat4 model;\n"
"uniform bool useOverride;\n"
"uniform vec3 overrideColor;\n"
"out vec4 fragColor;\n"
"void main() {\n"
"    gl_Position = model * vec4(position, 1.0);\n"
"    vec3 finalColor = useOverride ? overrideColor : color;\n"
"    fragColor = vec4(finalColor, 1.0);\n"
"}";

const char* fragmentShaderSource = "#version 450 core\n"
"in vec4 fragColor;\n"
"out vec4 color;\n"
"void main() {\n"
"    color = fragColor;\n"
"}";

int loadSimpleOBJ(string filePATH, int& nVertices) {
    std::vector<glm::vec3> vertices;
    std::vector<GLfloat> vBuffer;
    glm::vec3 color = glm::vec3(0.5f, 0.5f, 0.5f);

    std::ifstream file(filePATH);
    if (!file.is_open()) {
        cerr << "Erro ao abrir " << filePATH << endl;
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
            vertices.push_back(vert);
        } else if (word == "f") {
            for (int i = 0; i < 3; ++i) {
                ss >> word;
                int index = stoi(word) - 1;
                glm::vec3 v = vertices[index] * 0.2f;
                vBuffer.push_back(v.x);
                vBuffer.push_back(v.y);
                vBuffer.push_back(v.z);
                vBuffer.push_back(color.r);
                vBuffer.push_back(color.g);
                vBuffer.push_back(color.b);
            }
        }
    }

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    nVertices = vBuffer.size() / 6;
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
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Múltiplos OBJ", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetKeyCallback(window, key_callback);
    glEnable(GL_DEPTH_TEST);

    GLuint shader = createShaderProgram();
    glUseProgram(shader);

    GLint modelLoc = glGetUniformLocation(shader, "model");
    GLint useOverrideLoc = glGetUniformLocation(shader, "useOverride");
    GLint overrideColorLoc = glGetUniformLocation(shader, "overrideColor");

    std::vector<string> paths = {
		"assets\\Modelos3D\\Cube.obj",
		"assets\\Modelos3D\\Suzanne.obj"
    };

    for (int i = 0; i < paths.size(); ++i) {
        Mesh mesh;
        mesh.VAO = loadSimpleOBJ(paths[i], mesh.nVertices);
        mesh.translation = glm::vec3(0, 0, 0);
        mesh.rotation = glm::vec3(0);
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

            glBindVertexArray(mesh.VAO);
            glDrawArrays(GL_TRIANGLES, 0, mesh.nVertices);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
