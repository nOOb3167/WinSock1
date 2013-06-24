#include <cstdlib>
#include <cassert>

#include <functional>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
#include <deque>
#include <map>
#include <set>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <oglplus/all.hpp>

#define G_WIN_W 800
#define G_WIN_H 800

#define G_MAX_BONES_INFLUENCING 4

using namespace std;
using namespace oglplus;

class Ctx : public oglplus::Context {};

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

	/* http://msdn.microsoft.com/en-us/library/windows/desktop/dd183375%28v=vs.85%29.aspx
	If the height is negative, the bitmap is a top-down DIB and its origin is the upper left corner.
	*/
	assert(bInfo.bmiHeader.biWidth >= 0);

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

	/* Compensate for BGRA (Into RGBA) */
	for (int y = 0; y < bInfo.bmiHeader.biHeight; y++)
		for (int x = 0; x < bInfo.bmiHeader.biWidth; x++) {
			GLubyte tmp0 = buf[(y * bInfo.bmiHeader.biWidth * 4) + (x * 4) + 0];
			GLubyte tmp2 = buf[(y * bInfo.bmiHeader.biWidth * 4) + (x * 4) + 2];
			buf[(y * bInfo.bmiHeader.biWidth * 4) + (x * 4) + 0] = tmp2;
			buf[(y * bInfo.bmiHeader.biWidth * 4) + (x * 4) + 2] = tmp0;
		}

		r = DeleteDC(hdc);
		assert(r);

		r = DeleteObject(hBmp);
		assert(r);

		*w = tmpW;
		*h = tmpH;

		return buf;
};

oglplus::Texture * CreateTexture(const char *fname) {
	int w, h;
	SaneStoreState sss;

	unique_ptr<GLubyte[]> buf(CreateImageBufRGB(fname, &w, &h));
	shared_ptr<oglplus::images::Image> im = make_shared<oglplus::images::Image>(w, h, 1, 4, buf.get());

	oglplus::Texture *tex = new oglplus::Texture();
	tex->Active(0);
	tex->Bind(oglplus::TextureTarget::_2D);
	tex->MinFilter(oglplus::TextureTarget::_2D, oglplus::TextureMinFilter::Nearest);
	tex->MagFilter(oglplus::TextureTarget::_2D, oglplus::TextureMagFilter::Nearest);
	tex->Image2D(oglplus::TextureTarget::_2D, *im);
	tex->Unbind(oglplus::TextureTarget::_2D);

	return tex;
}

struct ExBase {
	virtual ~ExBase() {};
	virtual void Display() = 0;
};

bool VecSimilar(const aiVector3D &a, const aiVector3D &b) {
	const float delta = 0.001f;
	if (std::fabsf(a[0] - b[0]) < delta
		&& std::fabsf(a[1] - b[1]) < delta
		&& std::fabsf(a[2] - b[2]) < delta)
		return true;
	else return false;
}

bool MatSimilar(const aiMatrix4x4 &a, const aiMatrix4x4 &b) {
	const float delta = 0.001f;
	if (std::fabsf(a.a1 - b.a1) < delta && std::fabsf(a.a2 - b.a2) < delta && std::fabsf(a.a3 - b.a3) < delta && std::fabsf(a.a4 - b.a4) < delta &&
		std::fabsf(a.b1 - b.b1) < delta && std::fabsf(a.b2 - b.b2) < delta && std::fabsf(a.b3 - b.b3) < delta && std::fabsf(a.b4 - b.b4) < delta &&
		std::fabsf(a.c1 - b.c1) < delta && std::fabsf(a.c2 - b.c2) < delta && std::fabsf(a.c3 - b.c3) < delta && std::fabsf(a.c4 - b.c4) < delta &&
		std::fabsf(a.d1 - b.d1) < delta && std::fabsf(a.d2 - b.d2) < delta && std::fabsf(a.d3 - b.d3) < delta && std::fabsf(a.d4 - b.d4) < delta)
		return true;
	else return false;
}

bool MatSimilar3(const aiMatrix3x3 &a, const aiMatrix3x3 &b) {
	const float delta = 0.001f;
	if (std::fabsf(a.a1 - b.a1) < delta && std::fabsf(a.a2 - b.a2) < delta && std::fabsf(a.a3 - b.a3) < delta &&
		std::fabsf(a.b1 - b.b1) < delta && std::fabsf(a.b2 - b.b2) < delta && std::fabsf(a.b3 - b.b3) < delta &&
		std::fabsf(a.c1 - b.c1) < delta && std::fabsf(a.c2 - b.c2) < delta && std::fabsf(a.c3 - b.c3) < delta)
		return true;
	else return false;
}

oglplus::Mat4f MatOglplusFromAi(const aiMatrix4x4 &m) {
	return oglplus::Mat4f(
		m.a1, m.a2, m.a3, m.a4,
		m.b1, m.b2, m.b3, m.b4,
		m.c1, m.c2, m.c3, m.c4,
		m.d1, m.d2, m.d3, m.d4);
};

aiNode & CheckFindNode(const aiNode &n, const char *name) {
	/* FIXME: Grrr */
	aiNode *w = const_cast<aiNode &>(n).FindNode(name);
	assert(w);
	return *w;
}

struct Ex1 : public ExBase {
	aiScene *scene;
	shared_ptr<Texture> tex;

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

		tex = shared_ptr<Texture>(CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp"));

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
			fragColor = vec4(t.rgb, 1.0);\
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

		tex->Active(0);
		tex->Bind(oglplus::TextureOps::Target::_2D);

		vaCube.Unbind();	
	}

	void Display() {
		projM.Set(CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30));
		camM.Set(ModelMatrixf().TranslationZ(-2.0));
		mdlM.Set(ModelMatrixf());

		vaCube.Bind();

		tex->Active(0);
		tex->Bind(oglplus::TextureOps::Target::_2D);

		Ctx::DrawArrays(PrimitiveType::Triangles, 0, 3);

		vaCube.Unbind();
	}
};

Program * ShaderTexSimple() {
	VertexShader vs;
	FragmentShader fs;
	Program *prog = new Program();
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
	fs.Source(
		"#version 420\n\
		uniform sampler2D TexUnit;\
		in vec2 vTexCoord;\
		out vec4 fragColor;\
		void main(void) {\
		vec4 t = texture(TexUnit, vTexCoord);\
		fragColor = vec4(t.rgb, 1.0);\
		}"
		);
	vs.Compile();
	fs.Compile();
	prog->AttachShader(vs);
	prog->AttachShader(fs);
	prog->Link();
	return prog;
}

struct MeshData {
	size_t triCnt;

	vector<oglplus::Vec3f> vt;
	vector<oglplus::Vec2f> uv;
	vector<GLuint> id;

	MeshData(const vector<oglplus::Vec3f> &vt, const vector<oglplus::Vec2f> &uv, const vector<GLuint> &id) : vt(vt), uv(uv), id(id) {
		assert(vt.size() == uv.size() && vt.size() == id.size());
		assert((vt.size() % 3) == 0);
		triCnt = vt.size() / 3;
	}
};

struct MeshDataAnim {
	vector<string> bone;
	vector<oglplus::Mat4f> offsetMatrix;
	vector<array<float, G_MAX_BONES_INFLUENCING> > weight;

	MeshDataAnim(const vector<string> &bone, const vector<oglplus::Mat4f> &offsetMatrix, vector<array<float, G_MAX_BONES_INFLUENCING> > &weight) :
		bone(bone), offsetMatrix(offsetMatrix), weight(weight)
	{
		assert(bone.size() == offsetMatrix.size());
		/* FIXME: Maybe also check weights being zero or summing to 1.0 */
	}
};

MeshDataAnim MeshExtractAnim(const aiScene &s, const aiMesh &m) {

	assert(m.mNumBones <= G_MAX_BONES_INFLUENCING);

	const int numBone = m.mNumBones;
	const int numVert = m.mNumVertices;

	array<float, G_MAX_BONES_INFLUENCING> emPty;
	for (size_t i = 0; i < emPty.size(); i++) emPty[i] = 0.0;

	vector<string> bone;
	vector<oglplus::Mat4f> offsetMatrix;
	vector<array<float, G_MAX_BONES_INFLUENCING> > weight(numVert, emPty);

	for (int i = 0; i < numBone; i++)
		CheckFindNode(*s.mRootNode, m.mBones[i]->mName.C_Str());

	/* Construct 'bone' and 'offsetMatrix' */
	
	for (int i = 0; i < numBone; i++) {
		bone.push_back(string(m.mBones[i]->mName.C_Str()));
		offsetMatrix.push_back(MatOglplusFromAi(m.mBones[i]->mOffsetMatrix));
	}

	/* Prepare to construct 'weight' - Get the required data into a more suitable format first */

	struct WeightPair {
		unsigned int vertexId;
		float weight;
		WeightPair(unsigned int vertexId, float weight) : vertexId(vertexId), weight(weight) {}
	};

	vector<vector<WeightPair> > allPairs;

	for (int i = 0; i < numBone; i++) {
		const aiBone &b = *m.mBones[i];

		deque<WeightPair> wp;
		for (size_t j = 0; j < b.mNumWeights; j++)
			wp.push_back(WeightPair(b.mWeights[j].mVertexId, b.mWeights[j].mWeight));
		sort(wp.begin(), wp.end(), [](const WeightPair &a, const WeightPair &b) { return a.vertexId < b.vertexId; });

		vector<bool> present(numVert, false);
		for (size_t j = 0; j < wp.size(); j++)
			present.at(wp[j].vertexId) = true;

		/* 'wp' now [id0, ..., idn] ; where an aiWeight with id=idX was present in mWeights */
		/* 'present' now [bool0,...,booln] ; where true if aiWeight with id=idX was present in mWeights */

		/* To fill out a WeightPair vector where every 'id' from 0 to 'numVert' is present (Thus filling the gaps):
		Traverse present in order from 0 to numVert.
		false -> A gap, fill with a zero default.
		true  -> An existing weight. Get, then remove from 'wp'.
		Since the traversal is in order, and previous iterations popped all 'wp' members with a lower 'id',
		the wanted weight is at 'wp'.front. */

		vector<WeightPair> wpFull;
		for (int j = 0; j < numVert; j++) {
			if (present[j] == false) {
				/* FIXME: Should bones that are not influencing a particular vertex be getting a zero weight? (Likely yes) */
				wpFull.push_back(WeightPair(j, 0.0));
			} else {
				wpFull.push_back(wp.front());
				wp.pop_front();
			}
		}

		allPairs.push_back(move(wpFull));
	}

	/* Construct 'weight' */

	for (int i = 0; i < numVert; i++) {
		for (int j = 0; j < numBone; j++) {
			assert(allPairs[j][i].vertexId == i);
			weight[i][j] = allPairs[j][i].weight;
		}
	}

	return MeshDataAnim(move(bone), move(offsetMatrix), move(weight));
}

MeshData MeshExtract(const aiScene &s, const aiMesh &m) {
	vector<oglplus::Vec3f> vt;
	vector<oglplus::Vec2f> uv;
	vector<GLuint> id;

	for (size_t i = 0; i < m.mNumVertices; i++) {
		aiVector3D v = m.mVertices[i];
		vt.push_back(oglplus::Vec3f(v.x, v.y, v.z));
	}

	for (size_t i = 0; i < m.mNumVertices; i++) {
		aiVector3D v = m.mTextureCoords[0][i];
		uv.push_back(oglplus::Vec2f(v.x, v.y));
	}

	for (size_t i = 0; i < m.mNumFaces; i++) {
		assert(m.mFaces[i].mNumIndices == 3);
		for (size_t j = 0; j < 3; j++)
			id.push_back(m.mFaces[i].mIndices[j]);
	}

	return MeshData(vt, uv, id);
}

namespace Md {
	struct TexPair {
		shared_ptr<Buffer> uv;
		shared_ptr<Texture> tex;

		TexPair() : uv(new Buffer()), tex(new Texture()) {}
	};

	struct MdD {
		size_t triCnt;

		shared_ptr<Buffer> id;
		shared_ptr<Buffer> vt;
		TexPair tp;

		MdD(const MeshData &md, shared_ptr<Texture> tex) : triCnt(md.triCnt), id(new Buffer()), vt(new Buffer()) {

			id->Bind(oglplus::BufferOps::Target::Array);
			{
				vector<GLuint> v;
				for (auto &i : md.id) {
					v.push_back(i);
				}
				Buffer::Data(oglplus::BufferOps::Target::Array, v);
			}

			vt->Bind(oglplus::BufferOps::Target::Array);
			{
				vector<GLfloat> v;
				for (auto &i : md.vt) {
					v.push_back(i.x()); v.push_back(i.y()); v.push_back(i.z());
				}
				Buffer::Data(oglplus::BufferOps::Target::Array, v);
			}

			/* TexPair */

			tp.uv->Bind(oglplus::BufferOps::Target::Array);
			{
				vector<GLfloat> v;
				for (auto &i : md.uv) {
					v.push_back(i.x()); v.push_back(i.y());
				}
				Buffer::Data(oglplus::BufferOps::Target::Array, v);
			}

			tp.tex = tex;
		}
	};

	struct MdT {
		Mat4f ProjectionMatrix;
		Mat4f CameraMatrix;
		Mat4f ModelMatrix;

		MdT(const Mat4f &p, const Mat4f &c, const Mat4f &m) : ProjectionMatrix(p), CameraMatrix(c), ModelMatrix(m) {}
	};

	class Shd {
		bool valid;
	public:
		Shd() : valid(false) {}
		void Invalidate() { valid = false; }
		bool IsValid() { return valid; }
	protected:
		void Validate() { valid = true; }
	};

	class ShdTexSimple : public Shd {
	public:
		shared_ptr<Program> prog;
		shared_ptr<VertexArray> va;

		size_t triCnt;

		ShdTexSimple() :
			prog(shared_ptr<Program>(ShaderTexSimple())),
			va(new VertexArray()),
			triCnt(0) {}

		void Prime(const MdD &md, const MdT &mt) {
			/* MdD */

			triCnt = md.triCnt;

			va->Bind();

			/* FIXME: Really hard to navigate source but shader_data.hpp::_call_set_t indicates
			its 'program' argument is unused. Implies a program needs to be already bound at call time
			of (prog/blah) aka Uniform<blah>(prog, blah) = blah.
			But apparently not for ProgramUniform (OpenGL 4.1 or ARB_separate_shader_objects)
			UniformSetOps        ... ActiveProgramCallOps
			ProgramUniformSetOps ... SpecificProgramCallOps */
			/* prog.Use(); */

			md.id->Bind(oglplus::BufferOps::Target::ElementArray);

			md.vt->Bind(oglplus::BufferOps::Target::Array);
			(*prog|"Position").Setup(3, oglplus::DataType::Float).Enable();

			/* TexPair */

			md.tp.uv->Bind(oglplus::BufferOps::Target::Array);
			(*prog|"TexCoord").Setup(2, oglplus::DataType::Float).Enable();

			ProgramUniformSampler(*prog, "TexUnit") = 0;

			md.tp.tex->Active(0);
			md.tp.tex->Bind(oglplus::TextureOps::Target::_2D);

			/* MdT */

			ProgramUniform<Mat4f>(*prog, "ProjectionMatrix") = mt.ProjectionMatrix;
			ProgramUniform<Mat4f>(*prog, "CameraMatrix") = mt.CameraMatrix;
			ProgramUniform<Mat4f>(*prog, "ModelMatrix") = mt.ModelMatrix;

			Validate();
		}

		void Draw() {
			assert(IsValid());

			prog->Use();
			Ctx::DrawArrays(PrimitiveType::Triangles, 0, triCnt * 3);
			prog->UseNone();
		}

		void UnPrime() {
			Invalidate();

			va->Unbind();

			Texture::Unbind(oglplus::TextureOps::Target::_2D);
			Buffer::Unbind(oglplus::BufferOps::Target::Array);
			Buffer::Unbind(oglplus::BufferOps::Target::ElementArray);
		}
	};
}

void CheckUniqueNodeNames(const aiScene &s) {
	function<void (const aiNode &, set<string> *, int *)> helper = [&helper](const aiNode &n, set<string> *names, int *nodesMet) {
		(*nodesMet)++;
		names->insert(string(n.mName.C_Str()));

		for (int i = 0; i < n.mNumChildren; i++)
			helper(*n.mChildren[i], names, nodesMet);
	};

	set<string> names;
	int nodesMet = 0;

	assert(s.mRootNode);
	helper(*s.mRootNode, &names, &nodesMet);

	/* A duplicate insert would result in lower size */
	assert(names.size() == nodesMet);
}

void CheckOrientPlaceMesh(const aiMesh &m) {
	assert(m.mPrimitiveTypes == 4 && m.mNumFaces == 1 && m.GetNumUVChannels() == 1 && m.mNumUVComponents[0] == 2 && m.mNumVertices == 3);
	assert(VecSimilar(m.mVertices[m.mFaces[0].mIndices[0]], aiVector3D(1.0, 0.0, 0.0)));
	assert(VecSimilar(m.mVertices[m.mFaces[0].mIndices[1]], aiVector3D(1.0, 1.0, 0.0)));
	assert(VecSimilar(m.mVertices[m.mFaces[0].mIndices[2]], aiVector3D(0.0, 0.0, 0.0)));
	assert(VecSimilar(m.mTextureCoords[0][m.mFaces[0].mIndices[0]], aiVector3D(1.0, 0.0, 0.0)));
	assert(VecSimilar(m.mTextureCoords[0][m.mFaces[0].mIndices[1]], aiVector3D(1.0, 1.0, 0.0)));
	assert(VecSimilar(m.mTextureCoords[0][m.mFaces[0].mIndices[2]], aiVector3D(0.0, 0.0, 0.0)));
}

void CheckOrientPlace(const aiScene &s) {
	CheckUniqueNodeNames(s);

	assert(s.mNumMeshes == 1);

	CheckOrientPlaceMesh(*s.mMeshes[0]);

	aiNode &fakeRn = *s.mRootNode;
	assert(fakeRn.mName == aiString("Scene") && fakeRn.mNumChildren == 1);

	aiNode &rn = *fakeRn.mChildren[0];
	assert(rn.mName == aiString("Cube") && rn.mNumChildren == 0);
	assert(rn.mNumMeshes == 1);
	assert(MatSimilar(rn.mTransformation, aiMatrix4x4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)));
}

void CheckBoneAnimation(const aiScene &s) {
	assert(s.mNumAnimations == 1);

	aiAnimation &an = *s.mAnimations[0];
	assert(an.mNumChannels == 1);

	aiNodeAnim &ana = *an.mChannels[0];
	assert(ana.mNodeName == aiString("Bone"));

	assert(ana.mNumPositionKeys == 2 && ana.mNumRotationKeys == 2 && ana.mNumScalingKeys == 2);

	aiMatrix3x3 mR0 = ana.mRotationKeys[0].mValue.GetMatrix();
	aiMatrix3x3 mR1 = ana.mRotationKeys[1].mValue.GetMatrix();

	assert(MatSimilar3(ana.mRotationKeys[0].mValue.GetMatrix(), aiMatrix3x3(
		1,0,0,
		0,0,-1,
		0,1,0)));

	assert(MatSimilar3(ana.mRotationKeys[1].mValue.GetMatrix(), aiMatrix3x3(
		0,0,1,
		1,0,0,
		0,1,0)));

	assert(VecSimilar(ana.mScalingKeys[0].mValue, aiVector3D(1,1,1)));
	assert(VecSimilar(ana.mScalingKeys[1].mValue, aiVector3D(1,1,1)));

	assert(VecSimilar(ana.mPositionKeys[0].mValue, aiVector3D(0,0,0)));
	assert(VecSimilar(ana.mPositionKeys[1].mValue, aiVector3D(0,0,0)));
}

void CheckBone(const aiScene &s) {
	CheckUniqueNodeNames(s);

	assert(s.mNumMeshes == 1);

	aiNode &fakeRn = *s.mRootNode;
	assert(fakeRn.mName == aiString("Scene") && fakeRn.mNumChildren == 2);

	aiNode &nCube = CheckFindNode(fakeRn, "Cube");
	aiNode &nArmature = CheckFindNode(fakeRn, "Armature");
	aiNode &nBone = CheckFindNode(fakeRn, "Bone");

	assert(MatSimilar(nCube.mTransformation, aiMatrix4x4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)));
	assert(MatSimilar(nArmature.mTransformation, aiMatrix4x4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)));

	/* Bone left-handed */
	assert(MatSimilar(nBone.mTransformation, aiMatrix4x4(
		1,0,0,0,
		0,0,-1,0,
		0,1,0,0,
		0,0,0,1)));

	CheckBoneAnimation(s);
}

struct Ex2 : public ExBase {
	aiScene *scene;

	Md::ShdTexSimple shdTs;
	shared_ptr<Md::MdD> mdd;
	shared_ptr<Md::MdT> mdt;

	int tick;

	Ex2() : tick(0) {
		scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\t01_OrientPlace.dae", 0));
		assert(scene);

		aiScene &s = *scene;
		aiMesh &m = *s.mMeshes[0];

		CheckOrientPlace(s);

		mdd = make_shared<Md::MdD>(MeshExtract(s, m), shared_ptr<Texture>(CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp")));
		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::CameraMatrix(),
			ModelMatrixf());
	}

	void Display() {
		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::Orbiting(oglplus::Vec3f(0, 0, 0), 3, Degrees(float(tick * 5)), Degrees(15)),
			ModelMatrixf());
		tick++;

		shdTs.Prime(*mdd, *mdt);
		shdTs.Draw();
		shdTs.UnPrime();
	}
};

struct Ex3 : public ExBase {
	aiScene *scene;

	Ex3() {
		scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\t02_Bone.dae", 0));
		assert(scene);

		aiScene &s = *scene;
		aiMesh &m = *s.mMeshes[0];

		CheckBone(s);

		MeshDataAnim mda = MeshExtractAnim(s, m);
	}

	void Display() {
	}
};


void timerfunc(int msecTime) {
	glutPostRedisplay();
	glutTimerFunc(msecTime, timerfunc, msecTime);
}

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
			Ctx::ClearColor(0.2f, 0.2f, 0.2f, 0.0f);
			Ctx::Clear().ColorBuffer().DepthBuffer();

			gEx->Display();

			glutSwapBuffers();
		};

		glutDisplayFunc(dispfunc);
		glutTimerFunc(33, timerfunc, 33);

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

	//RunExample<Ex1>(argc, argv);
	//RunExample<Ex2>(argc, argv);
	RunExample<Ex3>(argc, argv);

	return EXIT_SUCCESS;
}
