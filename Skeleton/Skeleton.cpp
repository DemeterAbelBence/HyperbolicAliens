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
// Nev    : 
// Neptun : 
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
#include <iostream>
#include <vector>

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
void adjustPointAfterMoving(vec3& p) {
	float lambda = sqrtf(-1 / lorentzProduct(p, p));
	p = p * lambda;
}
void adjustVectorAfterMoving(vec3& v, const vec3& p) {
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
	adjustPointAfterMoving(r);
	return r;
}
void movePoint(vec3& p, vec3& v, float t) {
	v = normalizeVector(v);
	v = p * sinhf(t) + v * coshf(t);
	p = p * coshf(t) + v * sinhf(t);

	adjustVectorAfterMoving(v, p);
	adjustPointAfterMoving(p);
}
vec3 rotateVector(vec3& p, vec3& v, float angle) {
	v = normalizeVector(v);
	vec3 n = createNormalVector(p, v);
	n = normalizeVector(n);
	vec3 r = v * cosf(angle) + n * sinf(angle);
	adjustVectorAfterMoving(r, p);
	return r;
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
	hCircle() { glGenBuffers(1, &hCircleVBO); }
	hCircle(float x, float y, float z, float rad, float r, float g, float b) {
		color = vec3(r, g, b);
		hRadius = rad;
		hPoint = vec3(x, y, z);
		hVector = createDirectionVector(hPoint);
		
		displayPosition();
		
		loadHCirclePoints();
		glGenBuffers(1, &hCircleVBO);
	}
	vec2 getPPoint() const { return pPoint; }
	vec3 getHyperbolicVector() const { return hVector; }
	vec3 getHyperbolicPoint() const { return hPoint; }
	float getRadius() const { return hRadius; }
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
		glBufferData(GL_ARRAY_BUFFER, numberOfAttributes * sizeof(float), data, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}
	void displayPosition() {
		/*std::cout << "vx: " << hVector.x << " , vy: " << hVector.y << " , vz:" << hVector.z << std::endl;
		std::cout << "px: " << hPoint.x << " , py: " << hPoint.y << " , vz:" << hPoint.z << std::endl;
		std::cout << "hVector length: " << length(hVector / length(hVector)) << std::endl;
		std::cout << "r: " << hRadius << std::endl << std::endl;*/
	}

	void move(float t) {
		movePoint(hPoint, hVector, t);
		loadHCirclePoints();
		create();
	}
	void rotate(bool left) {
		if(left)
			hVector = rotateVector(hPoint, hVector, M_PI / 16);
		else
			hVector = rotateVector(hPoint, hVector, -M_PI / 16);

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
		glGenBuffers(1, &circleVBO);
	}
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
	const float eyeballRadius = 0.1f;
	const float angle = M_PI / 4;

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

public:
	Eyes(){}
	Eyes(hCircle* body) {
		vec3 leftEyeballPos = createEyeballPosition(body, true);
		vec3 rightEyeballPos = createEyeballPosition(body, false);

		leftEyeball = new hCircle(leftEyeballPos.x, leftEyeballPos.y, leftEyeballPos.z,
			eyeballRadius, 1.0f, 1.0f, 1.0f);
		rightEyeball = new hCircle(rightEyeballPos.x, rightEyeballPos.y, rightEyeballPos.z,
			eyeballRadius, 1.0f, 1.0f, 1.0f);

		leftEyeball->create();
		rightEyeball->create();
	}
	void update(hCircle* body) {
		vec3 leftEyeballPos = createEyeballPosition(body, true);
		vec3 rightEyeballPos = createEyeballPosition(body, false);

		leftEyeball->setHyperbolicPoint(leftEyeballPos);
		rightEyeball->setHyperbolicPoint(rightEyeballPos);

		leftEyeball->loadHCirclePoints();
		rightEyeball->loadHCirclePoints();

		leftEyeball->create();
		rightEyeball->create();
	}
	void draw() const {
		leftEyeball->draw();
		rightEyeball->draw();
	}
	~Eyes() {
		delete leftEyeball;
		delete rightEyeball;
	}
};

class UFO {
	hCircle* body;
	hCircle* mouth;
	Eyes* eyes;

	vec3 hPoint;
	vec3 hVector;

	float radius;
	bool moving = false;
	bool rotatingLeft = false;
	bool rotatingRight = false;

private:
	void rotate(bool left) { body->rotate(left); }
	void move(float t) {
		body->move(t);
	}

public:
	UFO(float x, float y, float z, float rad, float r, float g, float b){
		radius = rad;
		body = new hCircle(x, y, z, rad, r, g, b);

		hPoint = body->getHyperbolicPoint();
		hVector = body->getHyperbolicVector();
		vec3 mouthPos = createPointByMoving(hPoint, hVector, radius);

		mouth = new hCircle(mouthPos.x, mouthPos.y, mouthPos.z, 0.2f, 0.0f, 0.0f, 0.0f);

		body->create();
		mouth->create();

		eyes = new Eyes(body);
	}
	void setMoving(bool value) { moving = value; }
	void setRotatingLeft(bool value) { rotatingLeft = value; }
	void setRotatingRight(bool value) { rotatingRight = value; }
	void update() {
		hPoint = body->getHyperbolicPoint();
		hVector = body->getHyperbolicVector();
		vec3 mouthPosition = createPointByMoving(hPoint, hVector, radius);
		mouth->setHyperbolicPoint(mouthPosition);
		mouth->loadHCirclePoints();
		mouth->create();

		eyes->update(body);

		if (moving) 
			move(0.05f);
		
		if (rotatingLeft)
			rotate(true);

		if (rotatingRight)
			rotate(false);
	}
	
	void draw() const {
		body->draw();
		mouth->draw();
		eyes->draw();
	}
	~UFO() {
		delete body;
		delete mouth;
		delete eyes;
	}
};

UFO* player;
pCircle* poincarePlane;

void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	player = new UFO(0.0f, 0.0f, 1.0f, 0.3f, 1.0f, 0.0f, 0.0f);

	poincarePlane = new pCircle(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
	poincarePlane->create();

	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0.5f, 0.5f, 0.5f, 0);     // background color
	glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer

	float MVPtransf[4][4] = { 1, 0, 0, 0,    // MVP matrix, 
							  0, 1, 0, 0,    // row-major!
							  0, 0, 1, 0,
							  0, 0, 0, 1 };

	int location = glGetUniformLocation(gpuProgram.getId(), "MVP");	// Get the GPU location of uniform variable MVP
	glUniformMatrix4fv(location, 1, GL_TRUE, &MVPtransf[0][0]);	// Load a 4x4 row-major float matrix to the specified location
	
	poincarePlane->draw();
	player->draw();

	glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	if (key == 'e') {
		player->setMoving(true);
		player->update();
	}
	if (key == 'f') {
		player->setRotatingRight(true);
		player->update();
	}
	if (key == 's') {
		player->setRotatingLeft(true);
		player->update();
	}
	glutPostRedisplay();
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
	if (key == 'e') {
		player->setMoving(false);
		player->update();
	}
	if (key == 'f') {
		player->setRotatingRight(false);
		player->update();
	}
	if (key == 's') {
		player->setRotatingLeft(false);
		player->update();

	}
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

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
}
