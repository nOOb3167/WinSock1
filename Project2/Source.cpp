#include <cstdlib>
#include <cassert>

#include <functional>
#include <algorithm>
#include <iostream>
#include <sstream>
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

/* 640k should be enough for anyone - IIRC GL 3.0 Guarantees 1024 */
#define G_MAX_BONES_UNIFORM     30
#define G_MAX_BONES_INFLUENCING 4

using namespace oglplus;
using namespace std;

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

::std::string ConvertIntString(int a) {
	::std::stringstream ss;
	ss << a;
	return ss.str();
}

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
	int tick;
	ExBase() : tick(-1) {}
	virtual ~ExBase() {};
	virtual void Display() { tick++; };
};

bool ScaZero(float a) {
	const float delta = 0.001f;
	return (std::fabsf(a) < delta);
}

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

oglplus::Mat4f MatFromSca(const oglplus::Vec3f &s) {
	return oglplus::Mat4f(
		s[0], 0, 0, 0,
		0, s[1], 0, 0,
		0, 0, s[2], 0,
		0, 0, 0, 1);
}

oglplus::Mat4f MatFromRot(const oglplus::Mat3f &r) {
	return oglplus::Mat4f(
		r.At(0,0), r.At(0,1), r.At(0,2), 0,
		r.At(1,0), r.At(1,1), r.At(1,2), 0,
		r.At(2,0), r.At(2,1), r.At(2,2), 0,
		0, 0, 0, 1);
}

oglplus::Mat4f MatFromPos(const oglplus::Vec3f &p) {
	return oglplus::Mat4f(
		1, 0, 0, p[0],
		0, 1, 0, p[1],
		0, 0, 1, p[2],
		0, 0, 0, 1);
}

oglplus::Mat4f MatOglplusFromAi(const aiMatrix4x4 &m) {
	return oglplus::Mat4f(
		m.a1, m.a2, m.a3, m.a4,
		m.b1, m.b2, m.b3, m.b4,
		m.c1, m.c2, m.c3, m.c4,
		m.d1, m.d2, m.d3, m.d4);
};

oglplus::Mat3f MatOglplusFromAi3(const aiMatrix3x3 &m) {
	return oglplus::Mat3f(
		m.a1, m.a2, m.a3,
		m.b1, m.b2, m.b3,
		m.c1, m.c2, m.c3);
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

struct WeightPair {
	unsigned int vertexId;
	float weight;

	WeightPair(unsigned int vertexId, float weight) :
		vertexId(vertexId),
		weight(weight) {}
};

struct WeightData {
	size_t numVert;
	size_t numBone;
	unique_ptr<size_t[]> idWeight;
	unique_ptr<float[]> wtWeight;

	WeightData(size_t numVert, size_t numBone) :
		numVert(numVert),
		numBone(numBone),
		idWeight(new size_t[numVert * numBone]),
		wtWeight(new float[numVert * numBone])
	{
		for (size_t i = 0; i < numVert * numBone; i++) {
			idWeight[i] = 0;
			wtWeight[i] = 0.0f;
		}
	}

	WeightData(const WeightData &rhs) :
		numVert(rhs.numVert),
		numBone(rhs.numBone),
		idWeight(new size_t[numVert * numBone]),
		wtWeight(new float[numVert * numBone])
	{
		for (size_t i = 0; i < numVert * numBone; i++) {
			idWeight[i] = rhs.idWeight[i];
			wtWeight[i] = rhs.wtWeight[i];
		}
	}


	WeightData(WeightData &&rhs) :
		numVert(rhs.numVert),
		numBone(rhs.numBone),
		idWeight(move(rhs.idWeight)),
		wtWeight(move(rhs.wtWeight)) {}

	size_t & GetRefIdAt(size_t v, size_t b) const {
		assert(v < numVert && b < numBone);
		return idWeight[(v * numBone) + b];
	}

	float & GetRefWtAt(size_t v, size_t b) const {
		assert(v < numVert && b < numBone);
		return wtWeight[(v * numBone) + b];
	}

private:
	WeightData & operator =(const WeightData &rhs);
};

void WeightFullCompactTo(size_t maxInfluencing, const WeightData &weightFull, WeightData *weight) {
	assert(maxInfluencing == weight->numBone); /* '<' should be fine, too */

	for (size_t i = 0; i < weightFull.numVert; i++) {
		size_t curInf = 0;
		for (size_t j = 0; j < weightFull.numBone; j++) {
			if (curInf == maxInfluencing)
				break;

			/* FIXME: floating point comparison.
			Additionally, may want to pick the highest influence entries. */
			if (weightFull.GetRefWtAt(i, j) != 0.0f) {
				weight->GetRefIdAt(i, curInf) = weightFull.GetRefIdAt(i, j);
				weight->GetRefWtAt(i, curInf) = weightFull.GetRefWtAt(i, j);
				curInf++;
			}
		}
	}
}

struct MeshDataAnim {
	vector<string> bone;
	vector<oglplus::Mat4f> offsetMatrix;
	WeightData weight;
	WeightData weightFull;

	MeshDataAnim(const vector<string> &bone, const vector<oglplus::Mat4f> &offsetMatrix, const WeightData &weightFull) :
		bone(bone),
		offsetMatrix(offsetMatrix),
		weight(weightFull.numVert, G_MAX_BONES_INFLUENCING),
		weightFull(weightFull)
	{
		assert(bone.size() == offsetMatrix.size());

		WeightFullCompactTo(G_MAX_BONES_INFLUENCING, this->weightFull, &weight);
	}
};

struct AnimChan {
	string nameAffected;
	vector<oglplus::Vec3f> keyPos;
	vector<oglplus::Mat3f> keyRot;
	vector<oglplus::Vec3f> keySca;

	AnimChan(const aiNodeAnim &na) {
		nameAffected = string(na.mNodeName.C_Str());

		assert(na.mNumPositionKeys == na.mNumRotationKeys && na.mNumPositionKeys == na.mNumScalingKeys);

		/* No scaling aloud. Quiet only. */
		for (size_t i = 0; i < na.mNumScalingKeys; i++)
			assert(VecSimilar(na.mScalingKeys[i].mValue, aiVector3D(1,1,1)));

		for (size_t i = 0; i < na.mNumPositionKeys; i++)
			keyPos.push_back(oglplus::Vec3f(na.mPositionKeys[i].mValue.x, na.mPositionKeys[i].mValue.y, na.mPositionKeys[i].mValue.z));

		for (size_t i = 0; i < na.mNumRotationKeys; i++)
			keyRot.push_back(MatOglplusFromAi3(na.mRotationKeys[i].mValue.GetMatrix()));

		for (size_t i = 0; i < na.mNumScalingKeys; i++)
			keySca.push_back(oglplus::Vec3f(na.mScalingKeys[i].mValue.x, na.mScalingKeys[i].mValue.y, na.mScalingKeys[i].mValue.z));
	}
};

struct AnimData {
	string name;
	vector<AnimChan> chan;

	AnimData (const aiAnimation &an) {
		name = string(an.mName.C_Str());
		for (size_t i = 0; i < an.mNumChannels; i++)
			chan.push_back(AnimChan(*an.mChannels[i]));
	};
};

class NodeMapEntry {
	string name;
	int    id;
	bool null;
public:
	NodeMapEntry(const string &name, const int &id, const bool &null = false) :
		name(name),
		id(id),
		null(null) {}

	bool IsNull() const {
		return null;
	}

	int GetId() const {
		return id;
	}

private:
	void SetNull() {
		null = true;
	}

	friend class NodeMap;
	friend oglplus::Mat4f ChainWalkTrafo      (const NodeMap &nodeMap, const vector<oglplus::Mat4f> &ot, size_t initial);
	friend void           TrafoUpdateFromAnim (const AnimData &ad, const NodeMap &nodeMap, vector<oglplus::Mat4f> *upd, int frame);
};

class MeshNode {
public:
	NodeMapEntry name;

	NodeMapEntry parent;
	vector<NodeMapEntry> children;

	oglplus::Mat4f trafo;

	MeshNode(const aiNode &node, const int &ownId) :
		name(node.mName.C_Str(), ownId),
		parent("INVALID_NAME", -1),
		trafo(MatOglplusFromAi(node.mTransformation))
	{
		if (node.mParent != nullptr)
			parent = NodeMapEntry(node.mParent->mName.C_Str(), -1);

		for (size_t i = 0; i < node.mNumChildren; i++)
			children.push_back(NodeMapEntry(node.mChildren[i]->mName.C_Str(), -1));
	}
};

/**
* Extract and hold the node hierarchy from an aiNode.
* vecNodes: First element is the apex. Its 'name' entry is marked as 'null' (Terminator)
*
* The apex does not participate in Chain Accumulation or forming of the OffsetMatrix.
* (This resulted in convention change (Originally the apex nodes' 'parent' entry was 'null'))
*
* = Examples =
* Chain: (Scene)(Blah)(Mesh)
*   Accumulation: (Blah)*(Cube)
*       Notice the Scene terminator node not being multiplied-in.
* Chain: (Scene)(Blah2)(Bone)
*   OffsetMatrix: (Mesh^-1)*(Blah2^-1)*(Blah)*(Cube)
*       No Scene again.
*/
class NodeMap {
public:
	typedef map<string, shared_ptr<MeshNode> > mapNodes_t;
	typedef vector<shared_ptr<MeshNode> > vecNodes_t;
private:
	mapNodes_t mapNodes;
	vecNodes_t vecNodes;

public:

	NodeMap(const aiNode &node) {
		mapNodes_t *mNodes = &mapNodes;
		vecNodes_t *vNodes = &vecNodes;

		/* In this pass: Able to fill-in id of the currently processed node:
		All node.name's members are filled. Of node.parent and node.children[i], only the namestring. */

		function<void (const aiNode &)> helper = [&helper, &mNodes, &vNodes](const aiNode &n) {
			string nodeName(n.mName.C_Str());

			int ownId = vNodes->size();

			shared_ptr<MeshNode> w = shared_ptr<MeshNode>(new MeshNode(n, ownId));

			mNodes->insert(make_pair(nodeName, w));
			vNodes->push_back(w);

			for (size_t i = 0; i < n.mNumChildren; i++)
				helper(*n.mChildren[i]);
		};

		helper(node);

		/* Extra pass: Using node.name lookups, fill the remaining */

		/* The first node is marked as terminator / null */
		if (vNodes->size())
			(*(*vNodes)[0]).name.SetNull();

		for (size_t i = 0; i < vNodes->size(); i++) {
			MeshNode &mn = *(*vNodes)[i];

			if (!mn.name.IsNull())
				mn.parent = NodeMapEntry(GetRefByName(mn.parent.name).name); /* FIXME: Copyconstructed */

			for (auto &j : mn.children) {
				assert(j.id == -1 && j.null == false);
				j = NodeMapEntry(GetRefByName(j.name).name);
			}
		}
	}

	const vecNodes_t & GetRefNodes() const {
		return vecNodes;
	}

	const MeshNode & GetRefByNum(const size_t &idx) const {
		return *vecNodes.at(idx);
	}

	MeshNode & GetRefByName(const string &name) const {
		auto it = mapNodes.find(name);
		assert(it != mapNodes.end());
		return *it->second;
	}

	MeshNode & GetRefByEntry(const NodeMapEntry &nme) const {
		return GetRefByName(nme.name);
	}

	vector<oglplus::Mat4f> GetOnlyTrafo() {
		vector<oglplus::Mat4f> onlyTrafo;

		for (auto &i : vecNodes)
			onlyTrafo.push_back(i->trafo);

		return move(onlyTrafo);
	}
};

oglplus::Mat4f ChainWalkTrafo(const NodeMap &nodeMap, const vector<oglplus::Mat4f> &ot, size_t initial) {
	oglplus::Mat4f ret;

	const MeshNode *toProc = &nodeMap.GetRefByNum(initial);

	while (!toProc->name.IsNull()) {
		ret = ot[toProc->name.id] * ret;
		toProc = &nodeMap.GetRefByEntry(toProc->parent);
	}

	return ret;
}

void TrafoConstructAccumulated(const NodeMap &nodeMap, const vector<oglplus::Mat4f> &ot, vector<oglplus::Mat4f> *upd) {
	assert(nodeMap.GetRefNodes().size() == ot.size());
	assert(upd->empty());

	for (size_t i = 0; i < ot.size(); i++) {
		upd->push_back(ChainWalkTrafo(nodeMap, ot, i));
	}
}

void TrafoUpdateFromAnim(const AnimData &ad, const NodeMap &nodeMap, vector<oglplus::Mat4f> *upd, int frame) {
	assert(nodeMap.GetRefNodes().size() == upd->size());

	for (auto &i : ad.chan) {
		size_t toProc = nodeMap.GetRefByName(i.nameAffected).name.id;

		assert(toProc < upd->size());
		assert(frame >= 0 && (int)i.keyPos.size() > frame && (int)i.keyRot.size() > frame && (int)i.keySca.size() > frame);

		oglplus::Mat4f w;

		/* Really should just be doing: |r[:,0]*s[0] r[:,1]*s[1] r[:,2]*s[2] p[:]| <cat> |0 0 0 1| */
		w = MatFromSca(i.keySca[frame]) * w;
		w = MatFromRot(i.keyRot[frame]) * w;
		w = MatFromPos(i.keyPos[frame]) * w;

		upd->at(toProc) = w;
	}
}

AnimData ExtractAnim(const aiScene &s, const aiAnimation &an) {
	return AnimData(an);
}

NodeMap NodeMapExtract(const aiScene &s, const aiNode &from) {
	return NodeMap(from);
}

void ExtractMeshAnimAllPairs(const aiMesh &m, vector<vector<WeightPair> > *allPairsOut) {
	const int numVert = m.mNumVertices;
	const int numBone = m.mNumBones;

	vector<vector<WeightPair> > allPairs;

	for (int i = 0; i < numBone; i++) {
		const aiBone &b = *m.mBones[i];

		deque<WeightPair> wp;
		for (size_t j = 0; j < b.mNumWeights; j++)
			wp.push_back(WeightPair(b.mWeights[j].mVertexId, b.mWeights[j].mWeight));
		sort(wp.begin(), wp.end(), [](const WeightPair &a, const WeightPair &b) { return a.vertexId < b.vertexId; });

		vector<bool> present(numVert, false);
		for (auto &i : wp)
			present.at(i.vertexId) = true;

		/* 'wp' now [id0, ..., idn] ; where an aiWeight with id=idX was present in mWeights */
		/* 'present' now [bool0,...,booln] ; where true if aiWeight with id=idX was present in mWeights */

		/* To fill out a WeightPair vector where every 'id' from 0 to 'numVert' is present (Thus filling the gaps):
		Traverse 'present' in order from 0 to numVert.
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

	*allPairsOut = move(allPairs);
}

vector<int> ExtractMeshAnimMapNameId(const NodeMap &nodeMap, const vector<string> &boneName) {
	vector<int> boneToId;

	for (auto &i : boneName)
		boneToId.push_back(nodeMap.GetRefByName(i).name.GetId());

	return move(boneToId);
}

WeightData ExtractMeshAnimWeightData(const aiMesh &m, const vector<int> &boneId) {
	const int numVert = m.mNumVertices;
	const int numBone = m.mNumBones;

	WeightData weight(numVert, numBone);

	/* Prepare to construct 'weight' - Get the required data into a more suitable format first */

	vector<vector<WeightPair> > allPairs;

	ExtractMeshAnimAllPairs(m, &allPairs);

	/* Construct 'weight' */

	for (int i = 0; i < numVert; i++) {
		for (int j = 0; j < numBone; j++) {
			assert(allPairs[j][i].vertexId == i);
			weight.GetRefIdAt(i, j) = boneId[j];
			weight.GetRefWtAt(i, j) = allPairs[j][i].weight;
		}
	}

	return move(weight);
}

MeshDataAnim ExtractMeshAnim(const aiScene &s, const aiMesh &m, const NodeMap &nodeMap) {
	const int numVert = m.mNumVertices;
	const int numBone = m.mNumBones;

	for (int i = 0; i < numBone; i++) {
		CheckFindNode(*s.mRootNode, m.mBones[i]->mName.C_Str());
		nodeMap.GetRefByName(m.mBones[i]->mName.C_Str());
	}

	vector<oglplus::Mat4f> offsetMatrix;

	for (int i = 0; i < numBone; i++)
		offsetMatrix.push_back(MatOglplusFromAi(m.mBones[i]->mOffsetMatrix));

	vector<string> boneName;

	for (int i = 0; i < numBone; i++)
		boneName.push_back(string(m.mBones[i]->mName.C_Str()));

	vector<int> boneId(move(ExtractMeshAnimMapNameId(nodeMap, boneName)));
	WeightData  weight(move(ExtractMeshAnimWeightData(m, boneId)));

	return move(MeshDataAnim(move(boneName), move(offsetMatrix), move(weight)));
}

MeshData ExtractMesh(const aiScene &s, const aiMesh &m) {
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

	struct MdA {

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

	class ShdZ {
		ShdZ() {}
		void Prime(const MdD &md, const MdT &mt) {}
		void Draw();
		void UnPrime();
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

	Program * ShaderTexBone() {
		VertexShader vs;
		FragmentShader fs;
		Program *prog = new Program();

		string defS("#version 420\n");
		defS.append("#define MAX_BONES ");      defS.append(ConvertIntString(G_MAX_BONES_UNIFORM)); defS.append("\n");
		defS.append("#define MAX_BONES_INFL "); defS.append(ConvertIntString(G_MAX_BONES_INFLUENCING)); defS.append("\n");

		string vsSrc(defS);
		vsSrc.append(
			"uniform mat4  MagicTrafo[MAX_BONES];"
			"in      ivec4 IdWeight;"
			"in      vec4  WtWeight;"
			"uniform mat4 ProjectionMatrix, CameraMatrix, ModelMatrix;"
			"in  vec4 Position;"
			"in  vec2 TexCoord;"
			"out vec2 vTexCoord;"
			"void main(void) {"
			"  vTexCoord = TexCoord;"
			"  gl_Position = ProjectionMatrix * CameraMatrix * ModelMatrix * Position;"
			"}"
			);

		string fsSrc(defS);
		fsSrc.append(
			"uniform sampler2D TexUnit;"
			"in  vec2 vTexCoord;"
			"out vec4 fragColor;"
			"void main(void) {"
			"  vec4 t = texture(TexUnit, vTexCoord);"
			"  fragColor = vec4(t.rgb, 1.0);"
			"}"
			);

		vs.Source(vsSrc);
		fs.Source(fsSrc);
		vs.Compile();
		fs.Compile();
		prog->AttachShader(vs);
		prog->AttachShader(fs);
		prog->Link();
		return prog;
	}

	class ShdTexBone : public Shd {
	public:
		shared_ptr<Program> prog;
		shared_ptr<VertexArray> va;

		size_t triCnt;

		ShdTexBone() :
			prog(shared_ptr<Program>(ShaderTexBone())),
			va(new VertexArray()),
			triCnt(0) {}

		void Prime(const MdD &md, const MdT &mt, const MdA &ma) {
			/* MdD */

			triCnt = md.triCnt;

			va->Bind();

			md.id->Bind(oglplus::BufferOps::Target::ElementArray);

			md.vt->Bind(oglplus::BufferOps::Target::Array);
			(*prog|"Position").Setup(3, oglplus::DataType::Float).Enable();

			/* TexPair */

			md.tp.uv->Bind(oglplus::BufferOps::Target::Array);
			(*prog|"TexCoord").Setup(2, oglplus::DataType::Float).Enable();

			ProgramUniformSampler(*prog, "TexUnit") = 0;

			md.tp.tex->Active(0);
			md.tp.tex->Bind(oglplus::TextureOps::Target::_2D);

			ProgramUniform<Mat4f>(*prog, "ProjectionMatrix") = mt.ProjectionMatrix;
			ProgramUniform<Mat4f>(*prog, "CameraMatrix") = mt.CameraMatrix;
			ProgramUniform<Mat4f>(*prog, "ModelMatrix") = mt.ModelMatrix;

			OptionalProgramUniform<Mat4f>(*prog, "MagicTrafo[0]") = ModelMatrixf();

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

		for (size_t i = 0; i < n.mNumChildren; i++)
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

	NodeMap nm(fakeRn);
}

struct Ex2 : public ExBase {
	aiScene *scene;

	Md::ShdTexSimple shdTs;
	shared_ptr<Md::MdD> mdd;
	shared_ptr<Md::MdT> mdt;

	Ex2() {
		scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\t01_OrientPlace.dae", 0));
		assert(scene);

		aiScene &s = *scene;
		aiMesh &m = *s.mMeshes[0];

		CheckOrientPlace(s);

		mdd = make_shared<Md::MdD>(ExtractMesh(s, m), shared_ptr<Texture>(CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp")));
		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::CameraMatrix(),
			ModelMatrixf());
	}

	void Display() {
		ExBase::Display();

		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::Orbiting(oglplus::Vec3f(0, 0, 0), 3, Degrees(float(tick * 5)), Degrees(15)),
			ModelMatrixf());

		shdTs.Prime(*mdd, *mdt);
		shdTs.Draw();
		shdTs.UnPrime();
	}
};

struct Ex3 : public ExBase {
	aiScene *scene;

	shared_ptr<Texture> tex;

	shared_ptr<NodeMap> nodeMap;
	shared_ptr<AnimData> animData;

	Md::ShdTexBone shd;
	shared_ptr<Md::MdD> mdd;
	shared_ptr<Md::MdT> mdt;
	shared_ptr<Md::MdA> mda;

	Ex3() {
		scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\t02_Bone.dae", 0));
		assert(scene);

		aiScene &s = *scene;
		aiMesh &m = *s.mMeshes[0];

		CheckBone(s);

		tex = shared_ptr<Texture>(CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp"));

		nodeMap = shared_ptr<NodeMap>(new NodeMap(NodeMapExtract(s, CheckFindNode(*s.mRootNode, "Scene"))));
		animData = shared_ptr<AnimData>(new AnimData(ExtractAnim(s, *s.mAnimations[0])));

		//TODO: Convert to mda
		//MeshDataAnim mda = MeshExtractAnim(s, m);

		mdd = make_shared<Md::MdD>(ExtractMesh(s, m), tex);
		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::CameraMatrix(),
			ModelMatrixf());
	}

	void Display() {
		ExBase::Display();

		vector<oglplus::Mat4f> onlyTrafo = nodeMap->GetOnlyTrafo();
		vector<oglplus::Mat4f> updTrafo = onlyTrafo;
		vector<oglplus::Mat4f> accTrafo;
		TrafoUpdateFromAnim(*animData, *nodeMap, &updTrafo, (tick % 10) < 5);
		TrafoConstructAccumulated(*nodeMap, updTrafo, &accTrafo);

		MeshData newMd = ExtractMesh(*scene, *scene->mMeshes[0]);
		const MeshNode &nodeCube = nodeMap->GetRefByName("Cube");
		const MeshNode &nodeBone = nodeMap->GetRefByName("Bone");
		MeshDataAnim dataAnim = ExtractMeshAnim(*scene, *scene->mMeshes[0], *nodeMap);
		assert(dataAnim.bone.at(0) == "Bone");
		oglplus::Mat4f magicTrafo = accTrafo[nodeBone.name.GetId()] * dataAnim.offsetMatrix[0];

		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::Orbiting(oglplus::Vec3f(0, 0, 0), 3, Degrees(float(tick * 5)), Degrees(15)),
			magicTrafo);

		shd.Prime(*mdd, *mdt, *mda);
		shd.Draw();
		shd.UnPrime();
	}
};

struct Ex4 : public ExBase {
	aiScene *scene;

	shared_ptr<Texture> tex;

	shared_ptr<NodeMap> nodeMap;
	shared_ptr<AnimData> animData;

	Md::ShdTexSimple shd;
	shared_ptr<Md::MdD> mdd;
	shared_ptr<Md::MdT> mdt;
	shared_ptr<Md::MdA> mda;

	Ex4() {
		scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\t03_BoneTwo.dae", aiProcess_LimitBoneWeights | aiProcess_ValidateDataStructure));
		assert(scene);

		aiScene &s = *scene;
		aiMesh &m = *s.mMeshes[0];

		CheckUniqueNodeNames(s);
		assert(s.mNumMeshes == 1);
		CheckOrientPlaceMesh(m);

		CheckFindNode(*s.mRootNode, "Cube");
		CheckFindNode(*s.mRootNode, "Armature");
		CheckFindNode(*s.mRootNode, "Bone");
		CheckFindNode(*s.mRootNode, "Bone2");

		tex = shared_ptr<Texture>(CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp"));

		nodeMap = shared_ptr<NodeMap>(new NodeMap(NodeMapExtract(s, CheckFindNode(*s.mRootNode, "Scene"))));
		animData = shared_ptr<AnimData>(new AnimData(ExtractAnim(s, *s.mAnimations[0])));

		mdd = make_shared<Md::MdD>(ExtractMesh(s, m), tex);
		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::CameraMatrix(),
			ModelMatrixf());
	}

	void Display() {
		ExBase::Display();

		MeshData newMd = ExtractMesh(*scene, *scene->mMeshes[0]);
		MeshDataAnim dataAnim = ExtractMeshAnim(*scene, *scene->mMeshes[0], *nodeMap);
		vector<int> boneId = ExtractMeshAnimMapNameId(*nodeMap, dataAnim.bone);

		vector<oglplus::Mat4f> onlyTrafo = nodeMap->GetOnlyTrafo();
		vector<oglplus::Mat4f> updTrafo = onlyTrafo;
		vector<oglplus::Mat4f> accTrafo;
		TrafoUpdateFromAnim(*animData, *nodeMap, &updTrafo, (tick % 10) < 5);
		TrafoConstructAccumulated(*nodeMap, updTrafo, &accTrafo);

		vector<oglplus::Mat4f> magicTrafo;
		for (size_t i = 0; i < boneId.size(); i++)
			magicTrafo.push_back(accTrafo[boneId[i]] * dataAnim.offsetMatrix[i]);

		for (size_t i = 0; i < newMd.vt.size(); i++) {
			oglplus::Vec4f basePos(newMd.vt[i], 1);
			oglplus::Vec4f defoPos(0, 0, 0, 1);
			for (size_t j = 0; j < dataAnim.weight.numBone; j++) {
				if (!ScaZero(dataAnim.weight.GetRefWtAt(i, j)))
					defoPos += dataAnim.weight.GetRefWtAt(i, j) * (magicTrafo[j] * basePos);
			}
			newMd.vt[i] = oglplus::Vec3f(defoPos[0], defoPos[1], defoPos[2]); /* FIXME: Divide by defoPos[3] I guess */
		}

		mdd = make_shared<Md::MdD>(newMd, tex);
		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::Orbiting(oglplus::Vec3f(0, 0, 0), 3, Degrees(float(tick * 5)), Degrees(15)),
			ModelMatrixf());

		shd.Prime(*mdd, *mdt);
		shd.Draw();
		shd.UnPrime();
	}
};

struct Ex5 : public ExBase {
	aiScene *scene;

	shared_ptr<Texture> tex;

	shared_ptr<NodeMap> nodeMap;
	shared_ptr<AnimData> animData;

	Md::ShdTexBone shd;
	shared_ptr<Md::MdD> mdd;
	shared_ptr<Md::MdT> mdt;
	shared_ptr<Md::MdA> mda;

	Ex5() {
		scene = const_cast<aiScene *>(aiImportFile("C:\\Users\\Andrej\\Documents\\BlendTmp\\t03_BoneTwo.dae", aiProcess_LimitBoneWeights | aiProcess_ValidateDataStructure));
		assert(scene);

		aiScene &s = *scene;
		aiMesh &m = *s.mMeshes[0];

		CheckUniqueNodeNames(s);

		tex = shared_ptr<Texture>(CreateTexture("C:\\Users\\Andrej\\Documents\\BlendTmp\\bTest01.bmp"));

		nodeMap = shared_ptr<NodeMap>(new NodeMap(NodeMapExtract(s, CheckFindNode(*s.mRootNode, "Scene"))));
		animData = shared_ptr<AnimData>(new AnimData(ExtractAnim(s, *s.mAnimations[0])));
	}

	void Display() {
		ExBase::Display();

		MeshData newMd = ExtractMesh(*scene, *scene->mMeshes[0]);
		MeshDataAnim dataAnim = ExtractMeshAnim(*scene, *scene->mMeshes[0], *nodeMap);
		vector<int> boneId = ExtractMeshAnimMapNameId(*nodeMap, dataAnim.bone);

		vector<oglplus::Mat4f> onlyTrafo = nodeMap->GetOnlyTrafo();
		vector<oglplus::Mat4f> updTrafo = onlyTrafo;
		vector<oglplus::Mat4f> accTrafo;
		TrafoUpdateFromAnim(*animData, *nodeMap, &updTrafo, (tick % 10) < 5);
		TrafoConstructAccumulated(*nodeMap, updTrafo, &accTrafo);

		vector<oglplus::Mat4f> magicTrafo;
		for (size_t i = 0; i < boneId.size(); i++)
			magicTrafo.push_back(accTrafo[boneId[i]] * dataAnim.offsetMatrix[i]);

		for (size_t i = 0; i < newMd.vt.size(); i++) {
			oglplus::Vec4f basePos(newMd.vt[i], 1);
			oglplus::Vec4f defoPos(0, 0, 0, 1);
			for (size_t j = 0; j < dataAnim.weight.numBone; j++) {
				if (!ScaZero(dataAnim.weight.GetRefWtAt(i, j)))
					defoPos += dataAnim.weight.GetRefWtAt(i, j) * (magicTrafo[j] * basePos);
			}
			newMd.vt[i] = oglplus::Vec3f(defoPos[0], defoPos[1], defoPos[2]); /* FIXME: Divide by defoPos[3] I guess */
		}

		mdd = make_shared<Md::MdD>(newMd, tex);
		mdt = make_shared<Md::MdT>(
			CamMatrixf::PerspectiveX(Degrees(90), GLfloat(G_WIN_W)/G_WIN_H, 1, 30),
			CamMatrixf::Orbiting(oglplus::Vec3f(0, 0, 0), 3, Degrees(float(tick * 5)), Degrees(15)),
			ModelMatrixf());

		shd.Prime(*mdd, *mdt, *mda);
		shd.Draw();
		shd.UnPrime();
	}
};

void OglGenErr(oglplus::Error &err) {
	std::cerr <<
		"Error (in " << err.GLSymbol() << ", " <<
		err.ClassName() << ": '" <<
		err.ObjectDescription() << "'): " <<
		err.what() <<
		" [" << err.File() << ":" << err.Line() << "] ";
	std::cerr << std::endl;
	auto i = err.Properties().begin(), e = err.Properties().end();
	for (auto &i : err.Properties())
		std::cerr << "<" << i.first << "='" << i.second << "'>" << std::endl;
	err.Cleanup();
};

void timerfunc(int msecTime) {
	glutPostRedisplay();
	glutTimerFunc(msecTime, timerfunc, msecTime);
}

template<typename ExType>
void RunExample(int argc, char **argv) {
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
	static bool gFailed = false;

	/* Cannot pass exceptions through FreeGlut,
	have to workaround with error flags and FreeGlut API / Option flags. */

	auto dispfunc = []() {
		bool failed = false;

		try {
			Ctx::ClearColor(0.2f, 0.2f, 0.2f, 0.0f);
			Ctx::Clear().ColorBuffer().DepthBuffer();

			gEx->Display();

			glutSwapBuffers();
		} catch(oglplus::CompileError &e) {
			std::cerr << e.Log();
			OglGenErr(e);
			failed = true;
		} catch(oglplus::Error &e) {
			OglGenErr(e);
			failed = true;
		}

		if (failed) {
			gFailed = true;
			glutLeaveMainLoop();
		}
	};

	glutDisplayFunc(dispfunc);
	glutTimerFunc(33, timerfunc, 33);

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glutMainLoop();

	if (gFailed)
		throw exception("Failed");
}

int main(int argc, char **argv) {

	//RunExample<Ex1>(argc, argv);
	//RunExample<Ex2>(argc, argv);
	//RunExample<Ex3>(argc, argv);
	//RunExample<Ex4>(argc, argv);
	RunExample<Ex5>(argc, argv);

	return EXIT_SUCCESS;
}
