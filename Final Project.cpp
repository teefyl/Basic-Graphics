#define _CRT_SECURE_NO_DEPRECATE
//Some Windows Headers (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include "maths_funcs.h"

// Assimp includes

#include <assimp/cimport.h> // C importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.


/*----------------------------------------------------------------------------
                   MESH TO LOAD
  ----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define MESH_FENCE "../Lab5/fence.3ds"
#define MESH_COW "../Lab5/Cow.3ds"
#define MESH_PLAIN "../Lab5/plain.3ds"
#define MESH_BARN "../Lab5/Barn.3ds"
#define MESH_WINDBODY "../Lab5/windbody.3ds"
#define MESH_WINDHEAD "../Lab5/windhead.3ds"
#define TEXTURE_NAME "../Lab5/wood.jpg"
#define TEXTURE_COW "../Lab5/Cow.png"
/*----------------------------------------------------------------------------
  ----------------------------------------------------------------------------*/

std::vector<float> g_vp, g_vn, g_vt;
int g_point_count = 0;
int cow_count, fence_count, barn_count, plain_count, body_count, head_count; 
// Macro for indexing vertex buffer
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

using namespace std;
GLuint shaderProgramID;
unsigned int mesh_vao = 0;
int width = 800;
int height = 600;
GLuint loc1, loc2, loc3;
GLfloat rotate_y = 0.0f;
mat4 trans = identity_mat4();
mat4 myMatrix = identity_mat4();
mat4 camMatrix = identity_mat4();
int mouseX = 0;
int mouseY = 0;
vec3 forward = vec3(0.0, 0.0, 40.0);
int j = 40;
#pragma region MESH LOADING
/*----------------------------------------------------------------------------
                   MESH LOADING FUNCTION
  ----------------------------------------------------------------------------*/

bool load_mesh (const char* file_name) {
  const aiScene* scene = aiImportFile (file_name, aiProcess_Triangulate); // TRIANGLES!
        fprintf (stderr, "ERROR: reading mesh %s\n", file_name);
  if (!scene) {
    fprintf (stderr, "ERROR: reading mesh %s\n", file_name);
    return false;
  }
  printf ("  %i animations\n", scene->mNumAnimations);
  printf ("  %i cameras\n", scene->mNumCameras);
  printf ("  %i lights\n", scene->mNumLights);
  printf ("  %i materials\n", scene->mNumMaterials);
  printf ("  %i meshes\n", scene->mNumMeshes);
  printf ("  %i textures\n", scene->mNumTextures);
  
  for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
    const aiMesh* mesh = scene->mMeshes[m_i];
    printf ("    %i vertices in mesh\n", mesh->mNumVertices);
    g_point_count = mesh->mNumVertices;
    for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
      if (mesh->HasPositions ()) {
        const aiVector3D* vp = &(mesh->mVertices[v_i]);
        //printf ("      vp %i (%f,%f,%f)\n", v_i, vp->x, vp->y, vp->z);
        g_vp.push_back (vp->x);
        g_vp.push_back (vp->y);
        g_vp.push_back (vp->z);
      }
      if (mesh->HasNormals ()) {
        const aiVector3D* vn = &(mesh->mNormals[v_i]);
        //printf ("      vn %i (%f,%f,%f)\n", v_i, vn->x, vn->y, vn->z);
        g_vn.push_back (vn->x);
        g_vn.push_back (vn->y);
        g_vn.push_back (vn->z);
      }
      if (mesh->HasTextureCoords (0)) {
        const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
        //printf ("      vt %i (%f,%f)\n", v_i, vt->x, vt->y);
        g_vt.push_back (vt->x);
        g_vt.push_back (vt->y);
      }
      if (mesh->HasTangentsAndBitangents ()) {
        // NB: could store/print tangents here
      }
    }
  }
  
  aiReleaseImport (scene);
  return true;
}

#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS

// Create a NULL-terminated string by reading the provided file
char* readShaderSource(const char* shaderFile) {   
    FILE* fp = fopen(shaderFile, "rb"); //!->Why does binary flag "RB" work and not "R"... wierd msvc thing?
    if ( fp == NULL ) { return NULL; }
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    char* buf = new char[size + 1];
    fread(buf, 1, size, fp);
    buf[size] = '\0';
    fclose(fp);
    return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
    GLuint ShaderObj = glCreateShader(ShaderType);
    if (ShaderObj == 0) {
        fprintf(stderr, "Error creating shader type %d\n", ShaderType);
        exit(0);
    }
	const char* pShaderSource = readShaderSource( pShaderText);
	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
    glCompileShader(ShaderObj);
    GLint success;
	// check for shader related errors using glGetShaderiv
    glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar InfoLog[1024];
        glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
        fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
        exit(1);
    }
	// Attach the compiled shader object to the program object
    glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
    shaderProgramID = glCreateProgram();
    if (shaderProgramID == 0) {
        fprintf(stderr, "Error creating shader program\n");
        exit(1);
    }

	// Create two shader objects, one for the vertex, and one for the fragment shader
    AddShader(shaderProgramID, "../Shaders/simpleVertexShader.txt", GL_VERTEX_SHADER);
    AddShader(shaderProgramID, "../Shaders/simpleFragmentShader.txt", GL_FRAGMENT_SHADER);

    GLint Success = 0;
    GLchar ErrorLog[1024] = { 0 };
	// After compiling all shader objects and attaching them to the program, we can finally link it
    glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
    glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
        exit(1);
	}
	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
    glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
    glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
    if (!Success) {
        glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
        fprintf(stderr, "Invalid shader program: '%s'\n", ErrorLog);
        exit(1);
    }
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
    glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS

void generateObjectBufferMesh() {
/*----------------------------------------------------------------------------
                   LOAD MESH HERE AND COPY INTO BUFFERS
  ----------------------------------------------------------------------------*/

	load_mesh (MESH_COW);
	cow_count = g_point_count;
	load_mesh(MESH_PLAIN);
	plain_count = g_point_count;
	load_mesh(MESH_WINDBODY);
	body_count = g_point_count;
	load_mesh(MESH_WINDHEAD);
	head_count = g_point_count;
	load_mesh(MESH_FENCE);
	fence_count = g_point_count;
	load_mesh(MESH_BARN);
	barn_count = g_point_count;
   
	unsigned int vp_vbo = 0;
	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");

	g_point_count = cow_count + fence_count + barn_count + plain_count + body_count + head_count;

	glGenBuffers (1, &vp_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, vp_vbo);
	glBufferData (GL_ARRAY_BUFFER, g_point_count * 3 * sizeof (float), &g_vp[0], GL_STATIC_DRAW);
	unsigned int vn_vbo = 0;
	
	glGenBuffers (1, &vn_vbo);
	glBindBuffer (GL_ARRAY_BUFFER, vn_vbo);
	glBufferData (GL_ARRAY_BUFFER, g_point_count * 3 * sizeof (float), &g_vn[0], GL_STATIC_DRAW);
	
//	This is for texture coordinates which you don't currently need, so I have commented it out
//	unsigned int vt_vbo = 0;
	//glGenBuffers (1, &vt_vbo);
//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
//	glBufferData (GL_ARRAY_BUFFER, g_point_count * 2 * sizeof (float), &g_vt[0], GL_STATIC_DRAW);
	
	unsigned int vao = 0;
	glBindVertexArray (vao);
	glEnableVertexAttribArray (loc1);
	glBindBuffer (GL_ARRAY_BUFFER, vp_vbo);
	glVertexAttribPointer (loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray (loc2);
	glBindBuffer (GL_ARRAY_BUFFER, vn_vbo);
	glVertexAttribPointer (loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

//	This is for texture coordinates which you don't currently need, so I have commented it out
//	glEnableVertexAttribArray (loc3);
//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
//	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	
}


#pragma endregion VBO_FUNCTIONS


void display(){

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable (GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc (GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor (0.5f, 0.5f, 0.5f, 1.0f);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram (shaderProgramID);


	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation (shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation (shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation (shaderProgramID, "proj");
	

	// PARENT - cow
	mat4 view = identity_mat4();
	mat4 persp_proj = perspective(45.0, (float)width/(float)height, 0.1, 100.0);
	mat4 model = translate(identity_mat4(), vec3(0.0f, 0.0f, 0.0f));
	view = camMatrix;
	glUniformMatrix4fv (proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv (view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv (matrix_location, 1, GL_FALSE, model.m);
	glDrawArrays (GL_TRIANGLES, 0, cow_count);

	//PLAIN
	view = identity_mat4();
	persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	mat4 model4 = translate(model, vec3(0.0f, -1.5f, 0.0f));
	view = camMatrix;
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model4.m);
	glDrawArrays(GL_TRIANGLES, cow_count, plain_count);

	//WINDMILL
	view = identity_mat4();
	persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	mat4 model5 = translate(model, vec3(0.0f, 1.0f, -18.5f));
	view = camMatrix;
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model5.m);
	glDrawArrays(GL_TRIANGLES, cow_count + plain_count, body_count);

	//WINDMILL HEAD
	view = identity_mat4();
	persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	mat4 model6 = translate(identity_mat4(), vec3(0.38f, 2.0f,-15.0f));
	//model6 = rotate_z_deg(model6, rotate_y);
	view = camMatrix;
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model6.m);
	glDrawArrays(GL_TRIANGLES, cow_count + plain_count +body_count, head_count);

	// fence
	view = identity_mat4();
	persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	mat4 model2 = translate(model, vec3(7.0f, -0.6f, 0.0f));
	view = camMatrix;
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model2.m);
	glDrawArrays(GL_TRIANGLES, cow_count + plain_count + body_count+ head_count, fence_count);
	

	//BARN
	view = identity_mat4();
	persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	mat4 model3 = translate(model, vec3(-6.0f, -0.3f, 0.0f));
	view = camMatrix;
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model3.m);
	glDrawArrays(GL_TRIANGLES, fence_count + cow_count + plain_count + body_count+ head_count, barn_count);
	
    glutSwapBuffers();
}


void updateScene() {	

	// Placeholder code, if you want to work with framerate
	// Wait until at least 16ms passed since start of last frame (Effectively caps framerate at ~60fps)
	static DWORD  last_time = 0;
	DWORD  curr_time = timeGetTime();
	float  delta = (curr_time - last_time) * 0.001f;
	if (delta > 0.03f)
		delta = 0.03f;
	last_time = curr_time;

	// rotate the model slowly around the y axis
	rotate_y+=0.05f;
	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	GLuint shaderProgramID = CompileShaders();
	// load mesh into a vertex buffer array
	generateObjectBufferMesh();
	
}

void keypress(unsigned char key, int x, int y) {

	switch (key) {
	case 'c': //scale
		trans = scale(trans, vec3(0.8f, 0.8f, 0.8f));
		break;
	case 'v': //scale
		trans = scale(trans, vec3(1.2f, 1.2f, 1.2f));
		break;
	case 'w': //rotate X axis
		trans = rotate_x_deg(trans, 10.0f);
		break;
	case 'e': //rotate X axis
		trans = rotate_x_deg(trans, -10.0f);
		break;
	case 'a'://rotate Y axis
		trans = rotate_y_deg(trans, 10.0f);
		break;
	case 's'://rotate Y axis
		trans = rotate_y_deg(trans, -10.0f);
		break;
	case 'z'://rotate Z axis
		trans = rotate_z_deg(trans, 10.0f);
		break;
	case 'x'://rotate Z axis
		trans = rotate_z_deg(trans, -10.0f);
		break;
	case 'p'://translate the X pos by .1
		trans = translate(trans, vec3(10.0f, 0.0f, 0.0f));
		break;
	case 'o'://translate the X pos by .1
		trans = translate(trans, vec3(-10.0f, 0.0f, 0.0f));
		break;
	case'#':
		camMatrix = rotate_y_deg(camMatrix, 5.0f);
		break;
	case'/':
		camMatrix = rotate_y_deg(camMatrix, -5.0f);
		break;
	case'=':
		camMatrix = rotate_x_deg(camMatrix, 5.0f);
		break;
	case']':
		camMatrix = rotate_x_deg(camMatrix, -5.0f);
		break;
	default:
		break;
	}
}

void special(int key, int x, int y)
{
	switch (key) {
	case GLUT_KEY_UP:
		camMatrix = translate(camMatrix, vec3(0.0f, 0.0f, 1.0f));
		break;
	case GLUT_KEY_DOWN:
		camMatrix = translate(camMatrix, vec3(0.0f, 0.0f, -1.0f));
		break;
	case GLUT_KEY_LEFT:
		camMatrix = translate(camMatrix, vec3(1.0f, 0.0f, 0.0f));
		break;
	case GLUT_KEY_RIGHT:
		camMatrix = translate(camMatrix, vec3(-1.0f, 0.0f, 0.0f));
		break;
	default:
		break;
	}
}

void mouse(int button, int state,
	int x, int y) {
	
	if (mouseX != 0 && mouseY != 0)
	{
		camMatrix = rotate_y_deg(camMatrix, mouseX - x);
	}


	mouseX = x;
	mouseY = y; 
	 

}

int main(int argc, char** argv){

	// Set up the window
	glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
    glutInitWindowSize(width, height);
    glutCreateWindow("Barnyard");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);
    glutSpecialFunc(special);
	glutMouseFunc(mouse);
	 // A call to glewInit() must be done after glut is initialized!
    GLenum res = glewInit();
	// Check for any errors
    if (res != GLEW_OK) {
      fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
      return 1;
    }
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
    return 0;
}











