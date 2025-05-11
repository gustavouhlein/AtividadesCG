#include <iostream>
#include <string>
#include <assert.h>
#include <vector>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Protótipos das funções
int setupShader();
int setupGeometry();
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

const GLuint WIDTH = 1000, HEIGHT = 1000;

const GLchar* vertexShaderSource = "#version 450\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec3 color;\n"
"uniform mat4 model;\n"
"uniform vec3 overrideColor;\n"
"uniform bool useOverride;\n"
"out vec4 finalColor;\n"
"void main()\n"
"{\n"
"    gl_Position = model * vec4(position, 1.0);\n"
"    finalColor = vec4(color * (useOverride ? 0.5 : 1.0), 1.0);\n"
"}\0";

const GLchar* fragmentShaderSource = "#version 450\n"
"in vec4 finalColor;\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"    color = finalColor;\n"
"}\n\0";

std::vector<glm::vec3> cubePositions = { glm::vec3(0.0f) };
std::vector<glm::vec3> translations = { glm::vec3(0.0f) };
std::vector<float> scales = { 1.0f };
std::vector<glm::vec3> rotations = { glm::vec3(0.0f) };
int selectedCube = 0;
const float minScale = 0.1f;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GL_TRUE);
        if (key == GLFW_KEY_TAB) selectedCube = (selectedCube + 1) % cubePositions.size();

        // Rotação individual
        if (key == GLFW_KEY_X) rotations[selectedCube].x += 0.1f;
        if (key == GLFW_KEY_Y) rotations[selectedCube].y += 0.1f;
        if (key == GLFW_KEY_Z) rotations[selectedCube].z += 0.1f;

        // Movimento
        if (key == GLFW_KEY_W) translations[selectedCube].y += 0.1f;
        if (key == GLFW_KEY_S) translations[selectedCube].y -= 0.1f;
        if (key == GLFW_KEY_A) translations[selectedCube].x -= 0.1f;
        if (key == GLFW_KEY_D) translations[selectedCube].x += 0.1f;

        // Escala
        if (key == GLFW_KEY_MINUS) scales[selectedCube] = std::max(minScale, scales[selectedCube] * 0.9f);
        if (key == GLFW_KEY_EQUAL) scales[selectedCube] *= 1.1f;

        // Novo cubo
        if (key == GLFW_KEY_N) {
            cubePositions.push_back(glm::vec3(0.0f));
            translations.push_back(glm::vec3(0.0f));
            scales.push_back(1.0f);
            rotations.push_back(glm::vec3(0.0f));
        }
    }
}

int main()
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Cubos Interativos - Gustavo", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetKeyCallback(window, key_callback);

    GLuint shaderID = setupShader();
    GLuint VAO = setupGeometry();
    glUseProgram(shaderID);

    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint overrideColorLoc = glGetUniformLocation(shaderID, "overrideColor");
    GLint useOverrideLoc = glGetUniformLocation(shaderID, "useOverride");

    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < cubePositions.size(); ++i) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i] + translations[i]);
            model = glm::rotate(model, rotations[i].x, glm::vec3(1, 0, 0));
            model = glm::rotate(model, rotations[i].y, glm::vec3(0, 1, 0));
            model = glm::rotate(model, rotations[i].z, glm::vec3(0, 0, 1));
            model = glm::scale(model, glm::vec3(scales[i]));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(useOverrideLoc, i == selectedCube);
            glUniform3f(overrideColorLoc, 0.0f, 0.0f, 0.0f);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glfwTerminate();
    return 0;
}

int setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "Vertex Shader Error:\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "Fragment Shader Error:\n" << infoLog << endl;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        cout << "Shader Linking Error:\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}


int setupGeometry()
{
	GLfloat vertices[] = {
		-0.5f,-0.5f, 0.5f, 1,0,0,  0.5f,-0.5f, 0.5f, 1,0,0,  0.5f,0.5f, 0.5f, 1,0,0,
		-0.5f,-0.5f, 0.5f, 1,0,0,  0.5f,0.5f, 0.5f, 1,0,0, -0.5f,0.5f, 0.5f, 1,0,0,

		-0.5f,-0.5f,-0.5f, 0,1,0,  0.5f,0.5f,-0.5f, 0,1,0,  0.5f,-0.5f,-0.5f, 0,1,0,
		-0.5f,-0.5f,-0.5f, 0,1,0, -0.5f,0.5f,-0.5f, 0,1,0,  0.5f,0.5f,-0.5f, 0,1,0,

		0.5f,-0.5f,-0.5f, 0,0,1,  0.5f,0.5f, 0.5f, 0,0,1,  0.5f,0.5f,-0.5f, 0,0,1,
		0.5f,-0.5f,-0.5f, 0,0,1,  0.5f,-0.5f, 0.5f, 0,0,1, 0.5f,0.5f, 0.5f, 0,0,1,

		-0.5f,-0.5f,-0.5f, 1,1,0, -0.5f,0.5f, 0.5f, 1,1,0, -0.5f,0.5f,-0.5f, 1,1,0,
		-0.5f,-0.5f,-0.5f, 1,1,0, -0.5f,-0.5f, 0.5f, 1,1,0, -0.5f,0.5f, 0.5f, 1,1,0,

		-0.5f,0.5f,-0.5f, 1,0,1,  0.5f,0.5f, 0.5f, 1,0,1, -0.5f,0.5f, 0.5f, 1,0,1,
		-0.5f,0.5f,-0.5f, 1,0,1,  0.5f,0.5f,-0.5f, 1,0,1,  0.5f,0.5f, 0.5f, 1,0,1,

		-0.5f,-0.5f,-0.5f, 0,1,1, -0.5f,-0.5f, 0.5f, 0,1,1, 0.5f,-0.5f, 0.5f, 0,1,1,
		-0.5f,-0.5f,-0.5f, 0,1,1,  0.5f,-0.5f, 0.5f, 0,1,1, 0.5f,-0.5f,-0.5f, 0,1,1,
	};

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return VAO;
}