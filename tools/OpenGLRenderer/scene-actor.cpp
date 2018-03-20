#include "stdafx.h"
#include "scene-actor.h"
#include "xml-load.h"
#include "../GeometryLib/matrix44.h"



SceneActor::SceneActor(tinyxml2::XMLElement* pNode)
{
	XMLElement* pChild;
	pChild= pNode->FirstChildElement(XML_TAG_TRANSFORM);
	if (pChild)
		XML::load(pChild, m_transform);
}

SceneActor::~SceneActor()
{
}

void SceneActor::setTransform()
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	double *pMatrix = getTransformMatrix().asArray();
	if (pMatrix)
		glMultMatrixd(pMatrix);
}

void SceneActor::restoreTransform()
{
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

Matrix44 SceneActor::getTransformMatrix() const
{
	Matrix44 mat, rot, scale,trans;
	rot.setRotation(m_transform.rotation());
	trans.setTranslation(m_transform.translation());
	scale.setScale(m_transform.scale());
	mat = trans*rot*scale;
	return mat;
}

void SceneActor::addLocalOffset(Vector3D& offset)
{
	Matrix44 mat = getTransformMatrix();
	Vector3D worldOffset = mat*offset;
	m_transform.setTranslation(m_transform.translation() + worldOffset);
}

void SceneActor::addWorldOffset(Vector3D& offset)
{
	m_transform.setTranslation(	m_transform.translation() + offset);
}

void SceneActor::addRotation(Quaternion& quat)
{
	m_transform.setRotation(m_transform.rotation() * quat);
}