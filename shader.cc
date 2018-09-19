#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/freeglut.h>

#include <iostream>
#include <algorithm>

#include <TooN/TooN.h>
#include <TooN/se3.h>

#include <pthread.h>
//Because fuck you, that's why.
void junk() {
  pthread_getconcurrency();
};

using namespace TooN;
using namespace std;


void compile_shader_and_add_to_program(GLuint program_handle, const string& shader_text, GLenum type)
{
    GLuint shader_handle = glCreateShader(type);

    if (shader_handle == 0) 
	{
        cerr << "Error creating shader type: " <<   type << endl;
        exit(0);
    }
	
	const GLchar* text = shader_text.c_str();
	GLint length = shader_text.size();
    glShaderSource(shader_handle, 1, &text, &length);

    glCompileShader(shader_handle);

    GLint success;
    glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &success);
	
    if (!success) 
	{
        GLchar error[1024];
        glGetShaderInfoLog(shader_handle, sizeof(error)-1, nullptr, error);
        cerr << "Error compiling shader type " << type << ": " << error << endl;
        exit(1);
    }

    glAttachShader(program_handle, shader_handle);
}


#define checkError() checkError_(__FILE__, __LINE__)

void checkError_(const char *file, int line)
{

    GLenum glErr;

    glErr = glGetError();
    if (glErr != GL_NO_ERROR)
    {
        printf("glError in file %s @ line %d: %s\n",
			     file, line, gluErrorString(glErr));
        exit(1);
    }
}


int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
	glutInitWindowSize(1300, 1300);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Tutorial 04");

	// Must be done after glut is initialized!
	// and MUST be done before any shader stuff is done.
	GLenum res = glewInit();
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}


const char* vertexShaderCode=R"XX(
#version 330

in vec3 Position;
uniform mat4 trans;


void main()
{
	gl_Position =  trans * vec4(0.7 * Position.x, 0.7 * Position.y, Position.z, 1.0);
}
)XX";


	const char* fragmentShaderCode=R"(
#version 330

out vec4 FragColor;
in float stipple;
uniform float time;

vec3 hash3(ivec3 v)
{
	vec3 r = sin(v*1234.567890123 + 271.82818284);
	return fract(vec3(r.x+r.y*r.z, r.z+r.x*r.y, r.y+r.z*r.x)*29347.3245)*2-1;
}


#define PERLIN(X)                                                                  \
/*Hash the inputs to get a prng gradient */                                        \
float perlin##X(vec##X pos)                                                        \
{                                                                                  \
	/*Find the extreme corners of the surrounding cube  */                         \
	ivec##X below = ivec##X(floor(pos));                                           \
                                                                                   \
	/*The function value at each cornder of the surrounding cube */                \
	float values[1<<X];                                                            \
                                                                                   \
	for(uint i=0u; i < (1u<<X##u); i++)                                            \
	{                                                                              \
		/*Generate the vertex position.                                   */       \
                                                                                   \
		/*The d'th bit of i indicates whether we are at relative 0 or 1 along */   \
		/*  the d'th axis. */                                                      \
		ivec##X vertex;                                                            \
		for(uint d=0u; d < X##u; d++)                                              \
			vertex[d] = below[d] + int(bool(i & (1u<<d)));                         \
                                                                                   \
		/*Perlin noise is random gradients  */                                     \
		vec##X gradient =  hash##X(vertex);                                        \
                                                                                   \
		/*zip along the gradient to get the value */                               \
		values[i] = dot(gradient, pos - vertex);                                   \
	}                                                                              \
                                                                                   \
	for(int dim=X-1; dim >= 0; dim--)                                              \
	{                                                                              \
		/*modular distance along the axis */                                       \
		float t = pos[dim] - below[dim];                                           \
                                                                                   \
		/*Use an s-shaped fade function */                                         \
		float f = 1-t*t*(3 - 2*t);                                                 \
                                                                                   \
		/*Interpolate over the highest numbered dumensions first */                \
		for(int v=0; v < (1<<dim); v++)                                            \
			values[v] = mix(values[v], values[v | (1<<dim)], 1-f);                 \
	}                                                                              \
	return values[0];                                                              \
}

PERLIN(3)


void main()
{
	float a=0;
	
	vec3 pos3d = vec3(gl_FragCoord.x + time / 10, gl_FragCoord.y, time/10) * (1.0/128.0);
	for(int i=0; i < 7; i++)
		a +=  perlin3(pos3d * (1<<i)) / (1<<i);

    FragColor = vec4(a+.5,a+.5,a+.5,1);
}
)";	


	GLuint shader_prog = glCreateProgram();
	checkError();
	compile_shader_and_add_to_program(shader_prog, vertexShaderCode, GL_VERTEX_SHADER);
	checkError();
	compile_shader_and_add_to_program(shader_prog, fragmentShaderCode, GL_FRAGMENT_SHADER);
	checkError();
	glLinkProgram(shader_prog);
	checkError();
	
	glUseProgram(shader_prog);


	Vector<3, float> Vertices[] = 
	{
		makeVector(-1.0f, -1.0f, 0.0f),
		makeVector(1.0f, -1.0f, 0.0f),
		makeVector(0.0f, 1.0f, 0.0f)
	};

	
	SE3<float> rot;


	GLuint position_handle = glGetAttribLocation(shader_prog, "Position");
	glEnableVertexAttribArray(position_handle);
	glVertexAttribPointer(position_handle, 3, GL_FLOAT, false, 3*sizeof(float), Vertices);

	GLuint trans_handle = glGetUniformLocation(shader_prog, "trans");
	checkError();

	GLuint time_handle = glGetUniformLocation(shader_prog, "time");
	checkError();

	float matd[16];

	Matrix<4, 4, float, Reference::RowMajor> mat(matd);
	

	for(int i=0; ;i++)
	{
		rot = SE3<float>::exp(makeVector(0, 0, 0, 0, 0, i*0.01));
		mat = Identity;
		mat = rot * mat;
		cout << mat << endl;
		cout << i << endl << endl;

		glUniformMatrix4fv(trans_handle, 1, false, matd);
		checkError();

		glUniform1f(time_handle, i / 1.0);
		checkError();

		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glutSwapBuffers();

		cin.get();
	}
}
