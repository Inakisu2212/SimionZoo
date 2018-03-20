#pragma once

#include "app.h"
#include "../tools/WindowsUtils/Timer.h"
#define MAX_PATH_SIZE 1024

class Renderer;
class IInputHandler;
class Text2D;
class Meter2D;
using namespace std;

class RLSimionApp : public SimionApp
{
	//Rendering
	Timer m_timer;
	Renderer *m_pRenderer = 0;
	IInputHandler *m_pInputHandler = 0;
	void initRenderer(string sceneFile);
	void updateScene(State* s);
	Text2D* m_pProgressText= 0;
	vector<Meter2D*> m_pStatsText;
public:

	RLSimionApp(ConfigNode* pParameters);
	virtual ~RLSimionApp();

	void run();
};