#include "stdafx.h"
#include "critic.h"
#include "vfa.h"
#include "parameters.h"
#include "features.h"
#include "globals.h"
#include "experiment.h"

CTDCLambdaCritic::CTDCLambdaCritic(CParameters *pParameters)
{
	m_pVFA= new CRBFFeatureGridVFA(pParameters->getStringPtr("SIMGOD/CRITIC/VALUE_FUNCTION_RBF_VARIABLES"));
	m_z= new CFeatureList();
	m_aux= new CFeatureList();
	m_s_features = new CFeatureList();
	m_s_p_features = new CFeatureList();
	m_a = new CFeatureList();
	m_w = new CFeatureList();

	m_pAlpha= pParameters->add("SIMGOD/CRITIC/LEARNING_RATE",0.0);
	m_gamma= pParameters->getDouble("SIMGOD/CRITIC/INITIAL_GAMMA");
	m_lambda= pParameters->getDouble("SIMGOD/CRITIC/INITIAL_LAMBDA");
	m_beta = pParameters->getDouble("SIMGOD/CRITIC/INITIAL_BETA");

	if (pParameters->exists("SIMGOD/CRITIC/LOAD"))
		loadVFunction(pParameters->getStringPtr("SIMGOD/CRITIC/LOAD"));

	if (pParameters->exists("SIMGOD/CRITIC/SAVE"))
		strcpy_s(m_saveFilename,1024,pParameters->getStringPtr("SIMGOD/CRITIC/SAVE"));
	else
		m_saveFilename[0]= 0;
}

CTDCLambdaCritic::~CTDCLambdaCritic()
{
	if (m_saveFilename[0]!=0)
		saveVFunction(m_saveFilename);

	delete m_pVFA;
	delete m_z;
	delete m_aux;
	delete m_s_features;
	delete m_s_p_features;
	delete m_a;
	delete m_w;
}

double CTDCLambdaCritic::updateValue(CState *s, CAction *a, CState *s_p, double r, double rho)
{
	if (*m_pAlpha==0.0) return 0.0;
	
	if (g_pExperiment->m_step==1)
		m_z->clear();

	rho = 1.0;// no importance sampling for now

	m_pVFA->getFeatures(s, 0, m_s_features);
	m_pVFA->getFeatures(s_p, 0, m_s_p_features);

	//delta= r + gamma*omega(x_{t+1})- omega(x_t)
	double oldValue = m_pVFA->getValue(m_s_features);
	double newValue = m_pVFA->getValue(m_s_p_features);

	double td= r + m_gamma * newValue - oldValue;

	//a= omega(x_t)*w
	m_a->clear();
	m_a->copy(m_s_features);
	m_a->innerProduct(m_w);

	//z_{k+1}= rho*(omega(x_t) + gamma*lambda*z_k)
	m_z->mult(rho*m_lambda*m_gamma);
	m_z->addFeatureList(m_s_features,rho,false,true);
	m_z->applyThreshold(0.0001);	

	//theta= theta + alpha*td*z	
	m_pVFA->add(m_z, (*m_pAlpha) *td);//*rho);


	return td;
}

