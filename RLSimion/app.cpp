#include "stdafx.h"
#include "app.h"
#include "logger.h"
#include "worlds/world.h"
#include "experiment.h"
#include "SimGod.h"
#include "config.h"
#include "utils.h"
#include "../tools/WindowsUtils/FileUtils.h"

SimionApp* SimionApp::m_pAppInstance = 0;

SimionApp::SimionApp()
{
	m_pAppInstance = this;
}

SimionApp::~SimionApp()
{
	m_pAppInstance = 0;
	for (auto it = m_inputFiles.begin(); it != m_inputFiles.end(); it++) delete (*it);
	m_inputFiles.clear();
	for (auto it = m_outputFiles.begin(); it != m_outputFiles.end(); it++) delete (*it);
	m_outputFiles.clear();
	for (auto it = m_inputFilesRenamed.begin(); it != m_inputFilesRenamed.end(); it++)
	{
		if (*it != 0)
			delete *it;
	}
	m_inputFilesRenamed.clear();
}

const char* SimionApp::getArgValue(int argc,char** argv,char* argName)
{
	char argPrefix[256];
	sprintf_s(argPrefix, 256, "-%s=", argName);
	for (int i = 1; i < argc; ++i)
	{
		if (strstr(argv[i], argPrefix) == argv[i])
		{
			return (char*)(argv[i]) + strlen(argPrefix);
		}
	}
	return nullptr;
}

bool SimionApp::flagPassed(int argc, char** argv, char* flagName)
{
	char argPrefix[256];
	sprintf_s(argPrefix, 256, "-%s", flagName);
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], argPrefix))
		{
			return true;
		}
	}
	return false;
}

void SimionApp::printRequirements()
{
	const char *pFileName, *pFileRename;
	printf("<Files>\n");

	for (int i = 0; i < getNumInputFiles(); i++)
	{
		pFileName = getInputFile(i);
		pFileRename = getInputFileRename(i);
		if (pFileRename==0)
			printf("  <Input>%s</Input>\n", pFileName);
		else
			printf("  <Input Rename=\"%s\">%s</Input>\n", pFileRename, pFileName);
	}

	for (int i = 0; i < getNumOutputFiles(); i++)
	{
		pFileName = getOutputFile(i);
		printf("  <Output>%s</Output>\n", pFileName);
	}
	printf("</Files>\n");
}

SimionApp* SimionApp::get()
{
	return m_pAppInstance;
}

void SimionApp::setExecutedRemotely(bool remote)
{
	m_bRemoteExecution = remote;
}

bool SimionApp::isExecutedRemotely()
{
	return m_bRemoteExecution;
}

void SimionApp::setConfigFile(string configFile)
{
	//we provide the path to the xml configuration file so that the logger saves its log files in the directory
	m_directory = getDirectory(configFile);
	m_configFile = configFile;

	pLogger->setOutputFilenames();
}

string SimionApp::getConfigFile()
{
	return m_configFile;
}

string SimionApp::getOutputDirectory()
{
	return m_directory;
}




void SimionApp::registerInputFile(const char* filepath, const char* rename)
{
	char* copy = new char[strlen(filepath) + 1];
	strcpy_s(copy, strlen(filepath) + 1, filepath);
	m_inputFiles.push_back(copy);
	if (rename != 0)
	{
		copy = new char[strlen(rename) + 1];
		strcpy_s(copy, strlen(rename) + 1, rename);
		m_inputFilesRenamed.push_back(copy);
	}
	else m_inputFilesRenamed.push_back(0);
}

unsigned int SimionApp::getNumInputFiles()
{
	return (unsigned int) m_inputFiles.size();
}

const char* SimionApp::getInputFile(unsigned int i)
{
	if (i<m_inputFiles.size())
		return m_inputFiles[i];
	return 0;
}

const char* SimionApp::getInputFileRename(unsigned int i)
{
	if (i<m_inputFilesRenamed.size())
		return m_inputFilesRenamed[i];
	return 0;
}

unsigned int SimionApp::getNumOutputFiles()
{
	return (unsigned int) m_outputFiles.size();
}

const char* SimionApp::getOutputFile(unsigned int i)
{
	if (i<m_outputFiles.size())
		return m_outputFiles[i];
	return 0;
}


void SimionApp::registerOutputFile(const char* filepath)
{
	char* copy = new char[strlen(filepath) + 1];
	strcpy_s(copy, strlen(filepath) + 1, filepath);
	m_outputFiles.push_back(copy);
}

