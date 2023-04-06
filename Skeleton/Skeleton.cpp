//=============================================================================================
// Mintaprogram: Zöld háromszög. Ervenyes 2019. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Demeter Abel Bence
// Neptun : HHUZ6K
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
#include "framework.h"

// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters
const char * const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers

	uniform mat4 MVP;			// uniform variable, the Model-View-pProjection transformation matrix
	layout(location = 0) in vec2 vp;	// Varying input: vp = vertex position is expected in attrib array 0

	void main() {
		gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;		// transform vp from modeling space to normalized device space
	}
)";
	
// fragment shader in GLSL
const char * const fragmentSource = R"(
	#version 330			// Shader 3.3
	precision highp float;	// normal floats, makes no difference on desktop computers
	
	uniform vec3 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel

	void main() {
		outColor = vec4(color, 1);	// computed color is the color of the primitive
	}
)";

GPUProgram gpuProgram; // vertex and fragment shaders
unsigned int vao;	   // virtual world on the GPU

//hyperbolic geometry global functions
float lorentzProduct(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y - a.z * b.z; }
vec3 normalizeVector(vec3& v) {
	float length = sqrt(lorentzProduct(v, v));
	return v / length;
}
void adjustPoint(vec3& p) {
	float lambda = sqrtf(-1 / lorentzProduct(p, p));
	p = p * lambda;
}
void adjustVector(vec3& v, const vec3& p) {
	float lambda = lorentzProduct(v, p) / (p.z * p.z - p.x * p.x - p.y * p.y);
	v = v + p * lambda;
	v = normalizeVector(v);
}
vec3 createNormalVector(const vec3& p, vec3& v) {
	v = normalizeVector(v);
	return cross(vec3(v.x, v.y, -v.z), vec3(p.x, p.y, -p.z));
}
vec3 createPointByMoving(const vec3& p, vec3& v, float t) {
	v = normalizeVector(v);
	vec3 r = p * coshf(t) + v * sinhf(t);
	adjustPoint(r);
	return r;
}
void movePoint(vec3& p, vec3& v, float t) {
	v = normalizeVector(v);
	v = p * sinhf(t) + v * coshf(t);
	p = p * coshf(t) + v * sinhf(t);

	adjustVector(v, p);
	adjustPoint(p);
}
vec3 rotateVector(vec3& p, vec3& v, float angle) {
	v = normalizeVector(v);
	vec3 n = createNormalVector(p, v);
	n = normalizeVector(n);
	vec3 r = v * cosf(angle) + n * sinf(angle);
	adjustVector(r, p);
	return r;
}
float distanceBetween(const vec3& a, const vec3& b) {
	return acosh(1 + 2 * (lorentzProduct(-b, a)));
}
vec3 createDirectionVector(const vec3& p) {
	float vx, vy, vz;
	if (p.x != 0.0f) {
		vx = 1;
		vy = (p.z * p.z - vx * p.y) / p.x;
		vz = p.z;
		return vec3(vx, vy, vz);
	}
	if (p.y != 0.0f) {
		vx = (p.z * p.z - vy * p.x) / p.y;
		vy = 1;
		vz = p.z;
		return vec3(vx, vy, vz);
	}
	return vec3(0.0f, 1.0f, 0.0f);
}

//hyperbolic circle
class hCircle {

private:
	static const unsigned int numberOfAttributes = 50;
	static const unsigned int circlePoints = numberOfAttributes / 2;

	vec2 pPoint;
	vec2 pCirclePoints[circlePoints];

	vec3 hPoint;
	vec3 hCirclePoints[circlePoints];
	vec3 hVector;

	vec3 color;

	float hRadius;
	float data[numberOfAttributes];	
	unsigned int hCircleVBO;

public:
	hCircle() { }
	hCircle(float x, float y, float z, float rad, float r, float g, float b) {
		color = vec3(r, g, b);
		hRadius = rad;
		hPoint = vec3(x, y, z);
		hVector = createDirectionVector(hPoint);
				
		loadHCirclePoints();
	}
	void createVBO() { glGenBuffers(1, &hCircleVBO); }
	vec2 getPPoint() const { return pPoint; }
	vec3 getHyperbolicVector() const { return hVector; }
	vec3 getHyperbolicPoint() const { return hPoint; }
	vec3* getHyperbolicCirclePoints() const {
		vec3* result = new vec3[circlePoints];
		for (int i = 0; i < circlePoints; ++i)
			result[i] = hCirclePoints[i];
		
		return result;
	}
	float getRadius() const { return hRadius; }
	unsigned int getNumberOfCirclePoints() const { return circlePoints; }
	void setHyperbolicPoint(vec3 p) { hPoint = p; }
	
	void pProjection() {
		pPoint.x = hPoint.x / (hPoint.z + 1);
		pPoint.y = hPoint.y / (hPoint.z + 1);
		for (int i = 0; i < circlePoints; ++i) {
			pCirclePoints[i].x = hCirclePoints[i].x / (hCirclePoints[i].z + 1);
			pCirclePoints[i].y = hCirclePoints[i].y / (hCirclePoints[i].z + 1);
		}
	}
	void loadHCirclePoints() {
		vec3 r, m;
		int n = 0;
		for (int i = 0; i < circlePoints; ++i) {
			float angle = i * 2 * M_PI / circlePoints;
			r = rotateVector(hPoint, hVector, angle);
			vec3 m = createPointByMoving(hPoint, r, hRadius);
			hCirclePoints[i] = m;
		}
	}
	void updateGPU() const {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, color.x, color.y, color.z);

		glBindBuffer(GL_ARRAY_BUFFER, hCircleVBO);
		glBufferData(GL_ARRAY_BUFFER, numberOfAttributes * sizeof(float), data, GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	void move(float t) {
		movePoint(hPoint, hVector, t);
		loadHCirclePoints();
		create();
	}
	void rotate(bool left, float angle) {
		if(left)
			hVector = rotateVector(hPoint, hVector, angle);
		else
			hVector = rotateVector(hPoint, hVector, -angle);

		loadHCirclePoints();
		create();
	}
	void create() {
		pProjection();

		int i = 0;
		for (int k = 0; k < circlePoints; ++k) {
			data[i] = pCirclePoints[k].x;
			data[i + 1] = pCirclePoints[k].y;
			i += 2;
		}
	}
	void draw() const {
		updateGPU();
		glDrawArrays(GL_TRIANGLE_FAN, 0, circlePoints);
	}
};

//poincare circle
class pCircle {

private:
	static const unsigned int numberOfAttributes = 100;	
	static const unsigned int circlePoints = numberOfAttributes / 2;

	vec2 position;
	vec3 color;

	float radius;
	unsigned int circleVBO;
	float data[numberOfAttributes];

	void updateGPU() const  {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, color.x, color.y, color.z);

		glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
		glBufferData(GL_ARRAY_BUFFER, numberOfAttributes * sizeof(float), data, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}
public:
	pCircle(float x, float y, float rad, float r, float g, float b) {
		radius = rad;
		position.x = x;
		position.y = y;

		color = vec3(r, g, b);
	}
	void createVBO() { glGenBuffers(1, &circleVBO); }
	void create() {
		int i = 0;
		for (int j = 0; j < circlePoints; ++j) {
			float angle = j * 2 * M_PI / circlePoints;
			data[i] = radius * cos(angle);
			data[i + 1] = radius * sin(angle);
			i += 2;
		}
	}
	void draw() const {
		updateGPU();
		glDrawArrays(GL_TRIANGLE_FAN, 0, circlePoints);
	}
};

class Eyes {
	hCircle* leftEyeball;
	hCircle* rightEyeball;
	hCircle* leftPupil;
	hCircle* rightPupil;

	const float eyeballRadius = 0.1f;
	const float pupilRadius = 0.04f;
	const float angle = M_PI / 4;

	vec3 lookingAt;

private:
	vec3 createEyeballPosition(hCircle* body, bool left) const{
		vec3 p = body->getHyperbolicPoint();
		vec3 v = body->getHyperbolicVector();
		float r = body->getRadius();
		vec3 vr;

		if (left)
			vr = rotateVector(p, v, angle);
		else
			vr = rotateVector(p, v, -angle);

		return createPointByMoving(p, vr, r);
	}
	vec3 createPupilPosition(bool left) const {
		vec3* hCirclePoints;
		vec3 result;

		if (left)
			hCirclePoints = leftEyeball->getHyperbolicCirclePoints();
		else
			hCirclePoints = rightEyeball->getHyperbolicCirclePoints();

		int size = leftEyeball->getNumberOfCirclePoints();
		float min = 100;
		
		for (int i = 0; i < size; ++i) {
			float d = distanceBetween(hCirclePoints[i], lookingAt);
			if (d < min) {
				min = d;
				result = hCirclePoints[i];
			}
		}
		delete hCirclePoints;
		return result;
	}

public:
	Eyes() {}
	Eyes(hCircle* body, const vec3& l) {
		lookingAt = l;
		vec3 leftEyeballPos = createEyeballPosition(body, true);
		vec3 rightEyeballPos = createEyeballPosition(body, false);
		
		leftEyeball = new hCircle(leftEyeballPos.x, leftEyeballPos.y, leftEyeballPos.z,
			eyeballRadius, 1.0f, 1.0f, 1.0f);
		rightEyeball = new hCircle(rightEyeballPos.x, rightEyeballPos.y, rightEyeballPos.z,
			eyeballRadius, 1.0f, 1.0f, 1.0f);

		vec3 leftPupilPos = createPupilPosition(true);
		vec3 rightPupilPos = createPupilPosition(false);

		leftPupil = new hCircle(leftPupilPos.x, leftPupilPos.y, leftPupilPos.z,
			pupilRadius, 0.0f, 0.0f, 1.0f);
		rightPupil = new hCircle(rightPupilPos.x, rightPupilPos.y, rightPupilPos.z,
			pupilRadius, 0.0f, 0.0f, 1.0f);

		leftEyeball->create();
		rightEyeball->create();
		leftPupil->create();
		rightPupil->create();
	}
	void setLookingAt(const vec3& l) { lookingAt = l; }
	void createVBOs() { 
		leftEyeball->createVBO(); 
		rightEyeball->createVBO(); 
		leftPupil->createVBO();
		rightPupil->createVBO();
	}
	void update(hCircle* body) {
		vec3 leftEyeballPos = createEyeballPosition(body, true);
		vec3 rightEyeballPos = createEyeballPosition(body, false);
		vec3 leftPupilPos = createPupilPosition(true);
		vec3 rightPupilPos = createPupilPosition(false);

		leftEyeball->setHyperbolicPoint(leftEyeballPos);
		rightEyeball->setHyperbolicPoint(rightEyeballPos);
		leftPupil->setHyperbolicPoint(leftPupilPos);
		rightPupil->setHyperbolicPoint(rightPupilPos);

		leftEyeball->loadHCirclePoints();
		rightEyeball->loadHCirclePoints();
		leftPupil->loadHCirclePoints();
		rightPupil->loadHCirclePoints();

		leftEyeball->create();
		rightEyeball->create();
		leftPupil->create();
		rightPupil->create();
	}
	void draw() const {
		leftEyeball->draw();
		rightEyeball->draw();
		leftPupil->draw();
		rightPupil->draw();
	}
	~Eyes() {
		delete leftEyeball;
		delete rightEyeball;
		delete leftPupil;
		delete rightPupil;
	}
};

class Slime {

private:
	unsigned int slimeVBO;
	unsigned int numberOfAttributes;
	float width;

	float* data;

	std::vector<vec3> hPoints;
	std::vector<vec2> pPoints;
	vec3 color;

private:
	void updateGPU() const {
		int location = glGetUniformLocation(gpuProgram.getId(), "color");
		glUniform3f(location, color.x, color.y, color.z);

		glBindBuffer(GL_ARRAY_BUFFER, slimeVBO);
		glBufferData(GL_ARRAY_BUFFER, numberOfAttributes * sizeof(float), data, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}
	void projection() {
		pPoints.clear();
		unsigned int size = hPoints.size();
		for (int i = 0; i < size; ++i) {
			vec2 p;
			p.x = hPoints[i].x / (hPoints[i].z + 1);
			p.y = hPoints[i].y / (hPoints[i].z + 1);
			pPoints.push_back(p);
		}
	}

public:
	Slime() { width = 1; }
	Slime(float w, float r, float g, float b) {
		width = w;
		color = vec3(r, g, b);
	}
	void createVBO() { glGenBuffers(1, &slimeVBO); }
	void addPoint(const vec3& p) { hPoints.push_back(p); }
	void create() {
		projection();
		numberOfAttributes = pPoints.size() * 2;
		delete data;
		data = new float[numberOfAttributes];

		int j = 0;
		for (int i = 0; i < numberOfAttributes / 2; ++i) {
			data[j] = pPoints[i].x;
			data[j + 1] = pPoints[i].y;
			j += 2;
		}
	}
	void draw() const {
		updateGPU();
		glLineWidth(width);
		glDrawArrays(GL_LINE_STRIP, 0, pPoints.size());
	}
	~Slime() {
		delete data;
	}
};

class UFO {
	hCircle* body;
	hCircle* mouth;
	Eyes* eyes;
	Slime* slime;

	float mouthIterator = 0;

	float radius;
	bool moving = false;
	bool rotatingLeft = false;
	bool rotatingRight = false;
	bool mouthOpening = true;

private:
	void updateMouth(const vec3& p, vec3& v) {
		vec3 mouthPos;
		if (mouthIterator > 0.2f)
			mouthOpening = false;
		
		if (mouthIterator < 0.0f)
			mouthOpening = true;
		
		if (mouthOpening)
			mouthIterator += 0.0001f;
		else
			mouthIterator -= 0.0001f;

		mouthPos = createPointByMoving(p, v, radius - 0.1f + mouthIterator);
		mouth->setHyperbolicPoint(mouthPos);
		mouth->loadHCirclePoints();
		mouth->create();
	}

public:
	UFO(float x, float y, float z, float rad, float r, float g, float b){
		radius = rad;
		body = new hCircle(x, y, z, rad, r, g, b);

		vec3 p = body->getHyperbolicPoint();
		vec3 v = body->getHyperbolicVector();
		vec3 mouthPos = createPointByMoving(p, v, radius);

		mouth = new hCircle(mouthPos.x, mouthPos.y, mouthPos.z, 0.2f, 0.0f, 0.0f, 0.0f);

		body->create();
		mouth->create();

		eyes = new Eyes(body, vec3(2, 2, 3));

		slime = new Slime(3, 1.0f, 1.0f, 1.0f);
		slime->addPoint(p);
		slime->create();
	}
	void createVBOs() { 
		body->createVBO();
		eyes->createVBOs();
		mouth->createVBO();
		slime->createVBO();
	}
	void setMoving(bool value) { moving = value; }
	void setRotatingLeft(bool value) { rotatingLeft = value; }
	void setRotatingRight(bool value) { rotatingRight = value; }
	bool isMoving() const { return moving; }
	bool isRotatingLeft() const { return rotatingLeft; }
	bool isRotatingRight() const { return rotatingRight; }
	vec3 getBodyCentre() const { return body->getHyperbolicPoint(); }

	void rotate(bool left, float angle) { body->rotate(left, angle); }
	void move(float t) {
		body->move(t);
		vec3 p = body->getHyperbolicPoint();
		slime->addPoint(p);
		slime->create();
	}
	void update(const vec3& l) {
		vec3 p = body->getHyperbolicPoint();
		vec3 v = body->getHyperbolicVector();

		updateMouth(p, v);

		eyes->setLookingAt(l);
		eyes->update(body);
	}
	void draw() const {
		slime->draw();
		body->draw();
		mouth->draw();
		eyes->draw();
	}
	~UFO() {
		delete body;
		delete mouth;
		delete eyes;
		delete slime;
	}
};

UFO player(0.0f, 0.0f, 1.0f, 0.3f, 1.0f, 0.0f, 0.0f);
UFO npc(2.0f, 2.0f, 3.0f, 0.3f, 0.0f, 1.0f, 0.0f);
pCircle poincarePlane(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	player.createVBOs();
	npc.createVBOs();

	poincarePlane.createVBO();
	poincarePlane.create();

	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0.3f, 0.3f, 0.3f, 0);     // background color
	glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer

	float MVPtransf[4][4] = { 1, 0, 0, 0,    // MVP matrix, 
							  0, 1, 0, 0,    // row-major!
							  0, 0, 1, 0,
							  0, 0, 0, 1 };

	int location = glGetUniformLocation(gpuProgram.getId(), "MVP");	// Get the GPU location of uniform variable MVP
	glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);	// Load a 4x4 row-major float matrix to the specified location
	
	poincarePlane.draw();
	player.draw();
	npc.draw();

	glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == 'e')
		player.setMoving(true);

	if (key == 'f')
		player.setRotatingRight(true);
	
	if (key == 's')
		player.setRotatingLeft(true);

	glutPostRedisplay();
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
	if (key == 'e')
		player.setMoving(false);
	
	if (key == 'f') 
		player.setRotatingRight(false);
	
	if (key == 's')
		player.setRotatingLeft(false);

	glutPostRedisplay();
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {	// pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
	// Convert to normalized device space
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;
	printf("Mouse moved to (%3.2f, %3.2f)\n", cX, cY);
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
	// Convert to normalized device space
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;

	char * buttonStat;
	switch (state) {
	case GLUT_DOWN: buttonStat = "pressed"; break;
	case GLUT_UP:   buttonStat = "released"; break;
	}

	switch (button) {
	case GLUT_LEFT_BUTTON:   printf("Left button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY);   break;
	case GLUT_MIDDLE_BUTTON: printf("Middle button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY); break;
	case GLUT_RIGHT_BUTTON:  printf("Right button %s at (%3.2f, %3.2f)\n", buttonStat, cX, cY);  break;
	}
}


const float FPS = 60;
float frameTime = 1000 / FPS;
float previousTime = 0;
float currentTime = 0;
float deltaTime = 0;

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	long time = glutGet(GLUT_ELAPSED_TIME); 

	if (time % 20 == 0) {
		if (player.isMoving())
			player.move(0.01f);

		if (player.isRotatingLeft())
			player.rotate(true, 0.02f);

		if (player.isRotatingRight())
			player.rotate(false, 0.02f);

		npc.move(0.01f);
		npc.rotate(true, 0.02f);
	}
	player.update(npc.getBodyCentre());
	npc.update(player.getBodyCentre());
	glutPostRedisplay();
}
