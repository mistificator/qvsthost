#ifndef QVSTHOST_H
#define QVSTHOST_H

#include <QWidget>
#include <QString>
#include <QList>
#include <QVector>
#include <QSettings>
#include "./vstsdk/aeffectx.h"

class QVstPlugin
{
	friend class QVstChain;
	struct Data;
	Data * d;
public:
// ctor
	QVstPlugin();
	QVstPlugin(const QString & name, const QString & preset = QString());
	QVstPlugin(const QVstPlugin &);
	QVstPlugin & operator = (const QVstPlugin &);
// dtor
	~QVstPlugin();

// loading
	bool load(const QString & name);
	void setVstFileName(const QString & name);
	QString vstFileName() const;
	bool load();
	bool unload();
	bool isLoaded() const;

// low level
	const AEffect * lowLevelApi() const;

// start/stop
	void resume();
	void suspend();
	bool isSuspended() const;
	void setBypass(bool);
	bool bypass() const;

// common pars
	void setSampleRate(float);
	float sampleRate() const;
	void setBlockSize(int);
	int blockSize() const;

// gui
	QWidget * editWidget() const;
	void editOpen();
	void editClose();

// plugin properties
	int flags() const; // VstAEffectFlags
	int id() const;
	int pluginVersion() const;
	int vstVersion() const;
	int vendorVersion() const;
	QString effectName() const;
	VstPlugCategory category() const;

// inputs properties
	int inputsCount() const;
	QList<VstPinProperties> inputs() const;
	bool isGenerator() const;

// outputs properties
	int outputsCount() const;
	QList<VstPinProperties> outputs() const;

// programs(presets) properties
	int programsCount() const;
	void setProgram(int, const QString * new_program_name = NULL);
	int program(QString * program_name = NULL) const;
	QStringList programs() const;
	void savePreset(QSettings &) const;
	bool loadPreset(QSettings &);
	void savePreset(const QString &) const; // ini filename
	bool loadPreset(const QString &); // ini filename

// parameters properties
	int parametersCount() const;
	void setParameter(int, float);
	float parameter(int, VstParameterProperties * properties = NULL) const; 
	void setParameters(const QList<float> &);
	QList<float> parameters(QList<VstParameterProperties> * properties = NULL) const;

// queries
	bool canDo(const QString & canDoString);
	bool canProcessFloat() const;
	bool canProcessDouble() const;

// processing
	bool process(const float **, float **, int);
	bool process(const double **, double **, int);

	QList< QVector<float> > process(const QList< QVector<float> > & in = QList< QVector<float> >());
	QList< QVector<double> > process(const QList< QVector<double> > & in = QList< QVector<double> >());

	QVector<float> processOne(const QVector<float> & in = QVector<float>());
	QVector<double> processOne(const QVector<double> & in = QVector<double>());
};

// ---------------------------------------------------------------------------------

class QVstChain: public QList<QVstPlugin>
{
	struct Data;
	Data * d;
public:
// ctor
	QVstChain();
	QVstChain(const QString & preset);
	QVstChain(const QStringList & names);
	QVstChain(const QVstChain &);
	QVstChain & operator = (const QVstChain &);
// dtor
	~QVstChain();

// loading
	bool load(const QString &);
	bool load(const QStringList &);
	bool unload();

// start/stop
	void resume();
	void suspend();
	void setBypass(bool);

// common pars
	void setSampleRate(float);
	void setBlockSize(int);

// gui
	QWidgetList editWidgets() const;

// inputs properties
	int inputsCount() const;
	bool isGenerator() const;

// outputs properties
	int outputsCount() const;

// programs(presets) properties
	void savePreset(const QString &); // ini filename
	bool loadPreset(const QString &); // ini filename

// queries
	bool canProcessFloat() const;
	bool canProcessDouble() const;
	int linksCount() const; // available channels(links) to process, exclude first vst inputs
	bool canProcess(int) const;

// processing
	QList< QVector<float> > process(const QList< QVector<float> > & in = QList< QVector<float> >());
	QList< QVector<double> > process(const QList< QVector<double> > & in = QList< QVector<double> >());

	QVector<float> processOne(const QVector<float> & in = QVector<float>());
	QVector<double> processOne(const QVector<double> & in = QVector<double>());
};

#endif // QVSTHOST_H
