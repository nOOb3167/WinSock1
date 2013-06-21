#include <cstdlib>
#include <cassert>

#include <iostream>
#include <memory>
#include <vector>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <oglplus/all.hpp>

#define G_WIN_W 800
#define G_WIN_H 800

using namespace std;
using namespace oglplus;

class SaneStoreState {
public:
	GLint swapbytes, lsbfirst, rowlength, skiprows, skippixels, alignment;

	SaneStoreState() {
		glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swapbytes);
		glGetIntegerv(GL_UNPACK_LSB_FIRST, &lsbfirst);
		glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowlength);
		glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skiprows);
		glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skippixels);
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);

		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}

	~SaneStoreState() {
		glPixelStorei(GL_UNPACK_SWAP_BYTES, swapbytes);
		glPixelStorei(GL_UNPACK_LSB_FIRST, lsbfirst);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlength);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, skiprows);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, skippixels);
		glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
	}
};

GLubyte * CreateImageBufRGB(const char *fname, int *w, int *h) {
	int r;
	HBITMAP hBmp;
	HDC hdc;
	int tmpW, tmpH;
	GLubyte *buf;

	assert(w && h);

	hBmp = (HBITMAP)LoadImageA(NULL, fname, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	assert(hBmp);

	hdc = CreateCompatibleDC(NULL);
	assert(hdc);

	BITMAPINFO bInfo = {0};
	bInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	r = GetDIBits(hdc, hBmp, 0, 0, NULL, &bInfo, DIB_RGB_COLORS);
	assert(r);

	/* Only interested in Width and Height, now prime the structure for the 2nd GetDIBits call.
	http://msdn.microsoft.com/en-us/library/windows/desktop/dd183376%28v=vs.85%29.aspx
	If biBitCount = 32 && biCompression = BI_RGB then bmiColors will be empty.
	http://msdn.microsoft.com/en-us/library/windows/desktop/dd144879%28v=vs.85%29.aspx
	The scan lines must be aligned on a DWORD except for RLE compressed bitmaps.
	*/
	tmpW = bInfo.bmiHeader.biWidth;
	tmpH = bInfo.bmiHeader.biHeight;

	assert(bInfo.bmiHeader.biPlanes == 1);
	assert(bInfo.bmiHeader.biBitCount == 32);
	bInfo.bmiHeader.biCompression = BI_RGB;
	bInfo.bmiHeader.biSizeImage = 0;
	bInfo.bmiHeader.biClrUsed = 0;
	bInfo.bmiHeader.biClrImportant = 0;

	/* Times 4 is always DWORD aligned. */
	buf = new GLubyte[bInfo.bmiHeader.biWidth * bInfo.bmiHeader.biHeight * 4];
	assert (buf);

	r = GetDIBits(hdc, hBmp, 0, bInfo.bmiHeader.biHeight, buf, &bInfo, DIB_RGB_COLORS);
	assert(r);

	r = DeleteDC(hdc);
	assert(r);

	r = DeleteObject(hBmp);
	assert(r);

	*w = tmpW;
	*h = tmpH;

	return buf;
};

oglplus::Texture CreateTexture(const char *fname) {
	int w, h;
	SaneStoreState sss;

	unique_ptr<GLubyte[]> buf(CreateImageBufRGB(fname, &w, &h));
	shared_ptr<oglplus::images::Image> im = make_shared<oglplus::images::Image>(w, h, 1, 4, buf.get());

	oglplus::Texture tex;
	tex.Active(0);
	tex.Bind(oglplus::TextureTarget::_2D);
	tex.MinFilter(oglplus::TextureTarget::_2D, oglplus::TextureMinFilter::Nearest);
	tex.MagFilter(oglplus::TextureTarget::_2D, oglplus::TextureMagFilter::Nearest);
	tex.Image2D(oglplus::TextureTarget::_2D, *im);
	tex.Unbind(oglplus::TextureTarget::_2D);

	return tex;
}

struct ExBase {
	virtual void Display() = 0;
};

struct Ex1 : public ExBase {
	Context gl;

	aiScene *scene;
	Texture tex;

	VertexShader vs;
	FragmentShader fs;
	Program prog;

	LazyUniform<Mat4f> projM, camM, mdlM;

	VertexArray vaCube;
	Buffer vtCube, uvCube;

	Ex1() :
		projM(prog, "ProjectionMatrix"), 
		camM(prog, "CameraMatrix"), 
		mdlM(prog, "ModelMatrix")
	{
		scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\mCube01.dae", aiProcessPreset_TargetRealtime_MaxQuality));
		assert(scene);

		tex = CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp");

		vs.Source(
			"#version 420\n\
			uniform mat4 ProjectionMatrix, CameraMatrix, ModelMatrix;\
			in vec4 Position;\
			in vec2 TexCoord;\
			out vec2 vTexCoord;\
			void main(void) {\
			vTexCoord = TexCoord;\
			gl_Position = ProjectionMatrix * CameraMatrix * ModelMatrix * Position;\
			}"
			);
		vs.Compile();

		fs.Source(
			"#version 420\n\
			layout(binding = 0) uniform sampler2D TexUnit;\
			in vec2 vTexCoord;\
			out vec4 fragColor;\
			void main(void) {\
			vec4 t = texture(TexUnit, vTexCoord);\
			fragColor = vec4(1.0, t.gb, 1.0);\
			}"
			);
		fs.Compile();

		prog.AttachShader(vs);
		prog.AttachShader(fs);
		prog.Link();
		prog.Use();

		vaCube.Bind();

		vtCube.Bind(oglplus::BufferOps::Target::Array);
		{
			GLfloat v[] = { 0, 0, 0, 1, 0, 0, 1, 1, 0 };
			Buffer::Data(oglplus::BufferOps::Target::Array, v);
			(prog|"Position").Setup(3, oglplus::DataType::Float).Enable();
		}

		uvCube.Bind(oglplus::BufferOps::Target::Array);
		{
			GLfloat v[] = { 0, 0, 1, 0, 1, 1 };
			Buffer::Data(oglplus::BufferOps::Target::Array, v);
			(prog|"TexCoord").Setup(2, oglplus::DataType::Float).Enable();
		}

		tex.Active(0);
		tex.Bind(oglplus::TextureOps::Target::_2D);

		vaCube.Unbind();	
	}

	void Display() {
		projM.Set(CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30));
		camM.Set(ModelMatrixf().TranslationZ(-2.0));
		mdlM.Set(CamMatrixf());

		vaCube.Bind();

		tex.Active(0);
		tex.Bind(oglplus::TextureOps::Target::_2D);

		gl.DrawArrays(PrimitiveType::Triangles, 0, 3);

		vaCube.Unbind();
	}
};

template<typename ExType>
void RunExample(int argc, char **argv) {
	auto OglGenErr = [](oglplus::Error &err) {
		std::cerr <<
			"Error (in " << err.GLSymbol() << ", " <<
			err.ClassName() << ": '" <<
			err.ObjectDescription() << "'): " <<
			err.what() <<
			" [" << err.File() << ":" << err.Line() << "] ";
		std::cerr << std::endl;
		err.Cleanup();
	};

	try {
		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
		glutInitWindowSize(G_WIN_W, G_WIN_H);
		glutInitWindowPosition(100, 100);
		glutInitContextVersion (3,3);
		glutInitContextProfile (GLUT_CORE_PROFILE);
		glewExperimental=TRUE;
		glutCreateWindow("OGLplus");
		assert (glewInit() == GLEW_OK);
		glGetError();

		static ExBase *gEx = new ExType();

		auto dispfunc = []() {
			oglplus::Context gl;
			gl.ClearColor(0.2f, 0.2f, 0.2f, 0.0f);
			oglplus::Context::Clear().ColorBuffer().DepthBuffer();

			gEx->Display();

			glutSwapBuffers();
		};

		glutDisplayFunc(dispfunc);
		glutMainLoop();
	} catch(oglplus::CompileError &e) {
		std::cerr << e.Log();
		OglGenErr(e);
		throw;
	} catch(oglplus::Error &e) {
		OglGenErr(e);
		throw;
	}
}

int main(int argc, char **argv) {

	RunExample<Ex1>(argc, argv);

	return EXIT_SUCCESS;
}
