﻿#include "logger.h"
#include "worlds/world.h"
#include "named-var-set.h"
#include "config.h"
#include "stats.h"
#include "../tools/WindowsUtils/Timer.h"
#include "../tools/WindowsUtils/FileUtils.h"
#include "app.h"
#include "utils.h"
#include <algorithm>

FILE *CLogger::m_logFile = 0;
MessageOutputMode CLogger::m_messageOutputMode = MessageOutputMode::Console;
CNamedPipeClient CLogger::m_outputPipe;

#define HEADER_MAX_SIZE 16
#define EXPERIMENT_HEADER 1
#define EPISODE_HEADER 2
#define STEP_HEADER 3
#define EPISODE_END_HEADER 4

//we pack every int/double as 64bit data to avoid struct-padding issues (the size of the struct might not be the same in C++ and C#

struct ExperimentHeader
{
	__int64 magicNumber = EXPERIMENT_HEADER;
	__int64 fileVersion = CLogger::BIN_FILE_VERSION;
	__int64 numEpisodes = 0;

	__int64 padding[HEADER_MAX_SIZE - 3]; //extra space
	ExperimentHeader()
	{
		memset(padding, 0, sizeof(padding));
	}
};

struct EpisodeHeader
{
	__int64 magicNumber = EPISODE_HEADER;
	__int64 episodeType;
	__int64 episodeIndex;
	__int64 numVariablesLogged;

	//Added in version 2: if the episode belongs to an evaluation, the number of episodes per evaluation might be >1
	//the episodeSubIndex will be in [1..numEpisodesPerEvaluation]
	__int64 episodeSubIndex;

	__int64 padding[HEADER_MAX_SIZE - 5]; //extra space
	EpisodeHeader()
	{
		memset(padding, 0, sizeof(padding));
	}
};

struct StepHeader
{
	__int64 magicNumber = STEP_HEADER;
	__int64 stepIndex;

	double experimentRealTime;
	double episodeSimTime;
	double episodeRealTime;

	__int64 padding[HEADER_MAX_SIZE - 5]; //extra space
	StepHeader()
	{
		memset(padding, 0, sizeof(padding));
	}
};

CLogger::CLogger(CConfigNode* pConfigNode)
{
	if (!pConfigNode) return;

	m_bLogEvaluationEpisodes = BOOL_PARAM(pConfigNode, "Log-eval-episodes", "Log evaluation episodes?", true);

	m_bLogTrainingEpisodes = BOOL_PARAM(pConfigNode, "Log-training-episodes", "Log training episodes?", false);

	m_logFreq = DOUBLE_PARAM(pConfigNode, "Log-Freq", "Log frequency. Simulation time in seconds.", 0.25);

	m_pEpisodeTimer = new CTimer();
	m_pExperimentTimer = new CTimer();
	m_lastLogSimulationT = 0.0;
}

#define LOG_DESCRIPTOR_EXTENSION ".log"
#define LOG_BINARY_EXTENSION ".log.bin"

void CLogger::setOutputFilenames()
{
	string inputConfigFile = removeExtension(CSimionApp::get()->getConfigFile());

	//we register the names of the log files for input/output stuff
	m_outputLogDescriptor = inputConfigFile + LOG_DESCRIPTOR_EXTENSION;
	CSimionApp::get()->registerOutputFile(m_outputLogDescriptor.c_str());
	m_outputLogBinary = inputConfigFile + LOG_BINARY_EXTENSION;
	CSimionApp::get()->registerOutputFile(m_outputLogBinary.c_str());

	//open the log file
	openLogFile(m_outputLogBinary.c_str());
}


CLogger::~CLogger()
{
	if (m_pExperimentTimer) delete m_pExperimentTimer;
	if (m_pEpisodeTimer) delete m_pEpisodeTimer;

	m_outputPipe.closeConnection();

	for (auto it = m_stats.begin(); it != m_stats.end(); it++)
		delete *it;

	closeLogFile();
}


bool CLogger::isEpisodeTypeLogged(bool evalEpisode)
{
	return (evalEpisode && m_bLogEvaluationEpisodes.get()) || (!evalEpisode && m_bLogTrainingEpisodes.get());
}



void CLogger::writeLogFileXMLDescriptor(const char* filename)
{
	char buffer[BUFFER_SIZE];

	FILE * logXMLDescriptorFile;
	fopen_s(&logXMLDescriptorFile, filename, "w");
	if (logXMLDescriptorFile)
	{
		sprintf_s(buffer, BUFFER_SIZE, "<ExperimentLogDescriptor BinaryDataFile=\"%s\" SceneFile=\"%s\">\n"
			, getFilename(m_outputLogBinary).c_str()
			, (CSimionApp::get()->pWorld->getDynamicModel()->getName() + string(".scene")).c_str());
		writeEpisodeTypesToBuffer(buffer);
		writeNamedVarSetDescriptorToBuffer(buffer, "State", CSimionApp::get()->pWorld->getDynamicModel()->getStateDescriptorPtr()); //state
		writeNamedVarSetDescriptorToBuffer(buffer, "Action", CSimionApp::get()->pWorld->getDynamicModel()->getActionDescriptorPtr()); //action
		writeNamedVarSetDescriptorToBuffer(buffer, "Reward", CSimionApp::get()->pWorld->getRewardVector()->getPropertiesPtr());
		writeStatDescriptorToBuffer(buffer);
		strcat_s(buffer, BUFFER_SIZE, "</ExperimentLogDescriptor>");
		fwrite(buffer, 1, strlen(buffer), logXMLDescriptorFile);

		fclose(logXMLDescriptorFile);
	}
	else logMessage(MessageType::Warning, "Couldn't save experiment log descriptor");
}

void CLogger::writeEpisodeTypesToBuffer(char* pOutBuffer)
{
	if (m_bLogEvaluationEpisodes.get()) strcat_s(pOutBuffer, BUFFER_SIZE
		, "  <Episode-Type Id=\"0\">Evaluation</Episode-Type>\n");
	if (m_bLogTrainingEpisodes.get()) strcat_s(pOutBuffer, BUFFER_SIZE
		, "  <Episode-Type Id=\"1\">Training</Episode-Type>\n");
}

void CLogger::writeStatDescriptorToBuffer(char* pOutBuffer)
{
	char buffer[BUFFER_SIZE];

	for (auto iterator = m_stats.begin(); iterator != m_stats.end(); iterator++)
	{
		sprintf_s(buffer, BUFFER_SIZE, "  <Stat-variable>%s/%s</Stat-variable>\n", (*iterator)->getKey().c_str()
			, (*iterator)->getSubkey().c_str());
		strcat_s(pOutBuffer, BUFFER_SIZE, buffer);
	}
}
void CLogger::writeNamedVarSetDescriptorToBuffer(char* pOutBuffer, const char* id, const CDescriptor* descriptor)
{
	char buffer[BUFFER_SIZE];
	for (unsigned int i = 0; i < descriptor->size(); i++)
	{
		sprintf_s(buffer, BUFFER_SIZE, "  <%s-variable>%s</%s-variable>\n", id, (*descriptor)[i].getName(), id);
		strcat_s(pOutBuffer, BUFFER_SIZE, buffer);
	}
}

void CLogger::firstEpisode()
{
	//set episode start time
	m_pEpisodeTimer->start();

	//generate the xml descriptor of the log file
	writeLogFileXMLDescriptor(m_outputLogDescriptor.c_str());

	//write the log file header
	writeExperimentHeader();
}

void CLogger::lastEpisode()
{
}

void CLogger::firstStep()
{
	//initialise the episode reward
	m_episodeRewardSum = 0.0;

	//set episode start time
	m_pEpisodeTimer->start();

	m_lastLogSimulationT = 0.0;

	bool bEvalEpisode = CSimionApp::get()->pExperiment->isEvaluationEpisode();

	//reset stats
	for (auto it = m_stats.begin(); it != m_stats.end(); it++) (*it)->reset();

	if (isEpisodeTypeLogged(bEvalEpisode))
		writeEpisodeHeader();
}

void CLogger::lastStep()
{
	CExperiment* pExperiment = CSimionApp::get()->pExperiment.ptr();
	bool bEvalEpisode = pExperiment->isEvaluationEpisode();
	if (!isEpisodeTypeLogged(bEvalEpisode)) return;

	//log the end of the episode: this way we don't have to precalculate the number of steps logged per episode
	writeEpisodeEndHeader();

	//in case this is the last step of an evaluation episode, we log it and send the info to the host if there is one
	char buffer[BUFFER_SIZE];
	int episodeIndex = pExperiment->getEvaluationIndex();
	int numEvaluations = pExperiment->getNumEvaluations();
	int numEpisodesPerEvaluation = pExperiment->getNumEpisodesPerEvaluation();
	int numRelativeEpisodeIndex = pExperiment->getRelativeEpisodeIndex();

	//log the progress if an evaluation episode has ended
	if (pExperiment->isEvaluationEpisode()
		&& pExperiment->getEpisodeInEvaluationIndex() == pExperiment->getNumEpisodesPerEvaluation())
	{
		sprintf_s(buffer, BUFFER_SIZE, "%f,%f"
			, (double)(numRelativeEpisodeIndex - 1)	/ (std::max(1.0, (double)numEvaluations*numEpisodesPerEvaluation - 1))
			, m_episodeRewardSum / (double)pExperiment->getStep());
		logMessage(MessageType::Evaluation, buffer);
	}
}

void CLogger::timestep(CState* s, CAction* a, CState* s_p, CReward* r)
{
	bool bEvalEpisode = CSimionApp::get()->pExperiment->isEvaluationEpisode();
	//we add the scalar reward in evaluation episodes for monitoring purposes, no matter if we are logging this type of episode or not
	if (bEvalEpisode) m_episodeRewardSum += r->getSumValue();

	//update experiment stats
	for (auto iterator = m_stats.begin(); iterator != m_stats.end(); iterator++)
	{
		(*iterator)->addSample();
	}

	if (!isEpisodeTypeLogged(bEvalEpisode)) return;

	//output episode log data
	if (CSimionApp::get()->pWorld->getStepStartSimTime() - m_lastLogSimulationT >= m_logFreq.get()
		|| CSimionApp::get()->pExperiment->isFirstStep() || CSimionApp::get()->pExperiment->isLastStep())
	{
		writeStepData(s, a, s_p, r);
		//reset stats
		for (auto it = m_stats.begin(); it != m_stats.end(); it++) (*it)->reset();
		m_lastLogSimulationT = CSimionApp::get()->pWorld->getStepStartSimTime();
	}
}

void CLogger::writeStepData(CState* s, CAction* a, CState* s_p, CReward* r)
{
	int offset = 0;
	char buffer[BUFFER_SIZE];
	buffer[0] = 0;

	offset += writeStepHeaderToBuffer(buffer, offset);

	//We log s_p instead of s to log a coherent state-reward: r= f(s_p)
	offset += writeNamedVarSetToBuffer(buffer, offset, s_p);
	offset += writeNamedVarSetToBuffer(buffer, offset, a);
	offset += writeNamedVarSetToBuffer(buffer, offset, r);
	offset += writeStatsToBuffer(buffer, offset);

	writeLogBuffer(buffer, offset);
}

void CLogger::writeExperimentHeader()
{
	ExperimentHeader header;
	CExperiment* pExperiment = CSimionApp::get()->pExperiment.ptr();

	if (m_bLogEvaluationEpisodes.get())
		header.numEpisodes +=
		pExperiment->getNumEvaluations()*pExperiment->getNumEpisodesPerEvaluation();
	if (m_bLogTrainingEpisodes.get())
		header.numEpisodes += pExperiment->getNumTrainingEpisodes();

	writeLogBuffer((char*)&header, sizeof(ExperimentHeader));
}

void CLogger::writeEpisodeHeader()
{
	EpisodeHeader header;
	CExperiment* pExperiment = CSimionApp::get()->pExperiment.ptr();
	CWorld* pWorld = CSimionApp::get()->pWorld.ptr();

	header.episodeIndex = pExperiment->getRelativeEpisodeIndex();
	if (pExperiment->isEvaluationEpisode())
		header.episodeSubIndex = pExperiment->getEpisodeInEvaluationIndex();
	else
		header.episodeSubIndex = 1; // training episodes cannot have sub-episodes
	header.episodeType = (pExperiment->isEvaluationEpisode() ? 0 : 1);
	header.numVariablesLogged =
		pWorld->getDynamicModel()->getActionDescriptor().size()
		+ pWorld->getDynamicModel()->getStateDescriptor().size()
		+ pWorld->getRewardVector()->getNumVars()
		+ m_stats.size();

	writeLogBuffer((char*)&header, sizeof(EpisodeHeader));
}

void CLogger::writeEpisodeEndHeader()
{
	StepHeader episodeEndHeader;
	memset(&episodeEndHeader, 0, sizeof(StepHeader));
	episodeEndHeader.magicNumber = EPISODE_END_HEADER;
	writeLogBuffer((char*)&episodeEndHeader, sizeof(StepHeader));
}

int CLogger::writeStepHeaderToBuffer(char* buffer, int offset)
{
	StepHeader header;
	header.stepIndex = CSimionApp::get()->pExperiment->getStep();
	header.episodeRealTime = m_pEpisodeTimer->getElapsedTime();
	header.episodeSimTime = CSimionApp::get()->pWorld->getEpisodeSimTime();
	header.experimentRealTime = m_pExperimentTimer->getElapsedTime();

	memcpy_s(buffer + offset, BUFFER_SIZE, (char*)&header, sizeof(StepHeader));

	return sizeof(header);
}

int CLogger::writeNamedVarSetToBuffer(char* buffer, int offset, const CNamedVarSet* pNamedVarSet)
{
	int numVars = pNamedVarSet->getNumVars();
	double* pDoubleBuffer = (double*)(buffer + offset);
	for (int i = 0; i < numVars; ++i)
		pDoubleBuffer[i] = pNamedVarSet->get(i);
	return numVars * sizeof(double);
}

int CLogger::writeStatsToBuffer(char* buffer, int offset)
{
	int numVars = (int)m_stats.size();
	double* pDoubleBuffer = (double*)(buffer + offset);
	int i = 0;
	for (auto it = m_stats.begin(); it != m_stats.end(); ++it)
	{
		//Because we may not be logging all the steps, we need to save the average value from the last logged step
		//instead of only the current value
		pDoubleBuffer[i] = (*it)->getStatsInfo()->getAvg();
		++i;
	}
	return numVars * sizeof(double);
}


void CLogger::addVarSetToStats(const char* key, CNamedVarSet* varset)
{
	for (int i = 0; i < varset->getNumVars(); i++)
	{
		m_stats.push_back(new CStats<double>(key, varset->getProperties()[i].getName(), varset->getRef(i)));
	}
}

size_t CLogger::getNumStats()
{
	return m_stats.size();
}

IStats* CLogger::getStats(unsigned int i)
{
	if (i < m_stats.size())
		return m_stats[i];
	return nullptr;
}


void CLogger::openLogFile(const char* logFilename)
{
	fopen_s(&m_logFile, logFilename, "wb");
	if (!m_logFile)
		logMessage(MessageType::Warning, "Log file couldn't be opened, so no log info will be saved.");
}
void CLogger::closeLogFile()
{
	if (m_logFile)
		fclose(m_logFile);
}

void CLogger::writeLogBuffer(const char* pBuffer, int numBytes)
{
	size_t numBytesWritten = 0;
	if (m_logFile)
		numBytesWritten= fwrite(pBuffer, 1, numBytes, m_logFile);
}



void CLogger::logMessage(MessageType type, const char* message)
{
	char messageLine[1024];

	if (m_messageOutputMode == MessageOutputMode::NamedPipe && m_outputPipe.isConnected())
	{
		switch (type)
		{
		case Warning:
			sprintf_s(messageLine, 1024, "<Message>WARNING: %s</Message>", message); break;
		case Progress:
			sprintf_s(messageLine, 1024, "<Progress>%s</Progress>", message); break;
		case Evaluation:
			sprintf_s(messageLine, 1024, "<Evaluation>%s</Evaluation>", message); break;
		case Info:
			sprintf_s(messageLine, 1024, "<Message>%s</Message>", message); break;
		case Error:
			sprintf_s(messageLine, 1024, "<Error>ERROR: %s</Error>", message); break;
		}
		m_outputPipe.writeBuffer(messageLine, (int)strlen(messageLine) + 1);
	}
	else
	{
		switch (type)
		{
		case Warning:
			printf("WARNING: %s\n", message); break;
		case Progress:
			//extra spaces to avoid overwriting only partially previous message
			printf("PROGRESS: %s                     \r", message); break;
		case Evaluation:
			//extra spaces to avoid overwriting only partially previous message
			printf("EVALUATION: %s\n", message); break;
		case Info:
			printf("%s\n", message); break;
		case Error:
			printf("ERROR: %s\n", message); break;
		}
	}
	if (type == MessageType::Error)
		throw std::exception(message);
}
