#include "qvsthost.h"
#include <QLibrary>
#include <QDebug>
#include <Windows.h>

// C callbacks
extern "C" {
// Main host callback
VstIntPtr VSTCALLBACK hostCallback(AEffect *effect, VstInt32 opcode, VstInt32 /*index*/, VstIntPtr /*value*/, void *ptr, float /*opt*/)
{
	static const char product_string[] = "QVstHost";
	switch(opcode) 
	{
	case audioMasterVersion:
		return kVstVersion;
	case audioMasterCurrentId:
		return effect->uniqueID;
	case audioMasterIdle:
		return effect->dispatcher(effect, effEditIdle, 0, 0, 0, 0);
	case audioMasterGetVendorString:
	case audioMasterGetProductString:
		ptr = (void *)product_string;
		return 1;
	case audioMasterGetVendorVersion:
		return 1;
	case audioMasterCanDo:
		qDebug() << (char *)ptr;
		return 0;
	case audioMasterGetCurrentProcessLevel:
		return kVstProcessLevelUnknown;
	case audioMasterUpdateDisplay:
		return 0;
	case audioMasterAutomate:
		return 0;
	case 4 /*audioMasterPinConnected*/:
	case 6 /*audioMasterWantMidi*/:
	case 14 /*audioMasterNeedIdle*/:
		return 0;
	}
	qDebug() << "vst requested opcode" << opcode;
	return 0;
}
}

// plugin's entry point
typedef AEffect *(VSTCALLBACK *vstFuncPtr)(audioMasterCallback host);

struct QVstPlugin::Data
{
	QLibrary plugin;
	AEffect * aeffect;
	bool ok;
	QWidget * edit_widget;
	float samplerate;
	int blocksize;
	bool bypass;
	bool suspended;
	int chainindex;
	Data(): samplerate(8000), blocksize(4096), bypass(false), suspended(true), chainindex(0), ok (false)
	{
		edit_widget = new QWidget(0, Qt::Tool | Qt::MSWindowsOwnDC | Qt::MSWindowsFixedSizeDialogHint);
	}
	~Data()
	{
		edit_widget->deleteLater();
	}
};

QVstPlugin::QVstPlugin(): d(new Data())
{
}

QVstPlugin::QVstPlugin(const QString & name, const QString & preset): d(new Data())
{
	if (load(name))
	{
		if (!preset.isEmpty())
		{
			loadPreset(preset);
		}
	}
}

QVstPlugin::QVstPlugin(const QVstPlugin & o): d(new Data())
{
	setVstFileName(o.vstFileName());
	if (o.isLoaded())
	{
		load();
		setParameters(o.parameters());
	}
	d->samplerate = o.d->samplerate;
	d->blocksize = o.d->blocksize;
}

QVstPlugin & QVstPlugin::operator = (const QVstPlugin & o)
{
	if (& o != this)
	{
		setVstFileName(o.vstFileName());
		if (o.isLoaded())
		{
			unload();
			load();
			setParameters(o.parameters());
		}
		d->samplerate = o.d->samplerate;
		d->blocksize = o.d->blocksize;
	}
	return * this;
}

QVstPlugin::~QVstPlugin()
{
	unload();
	delete d;
}

bool QVstPlugin::load(const QString & name)
{
	setVstFileName(name);
	return load();
}

void QVstPlugin::setVstFileName(const QString & name)
{
	if (d->plugin.fileName() != name)
	{
		unload();
		d->plugin.setFileName(name);
	}
}

QString QVstPlugin::vstFileName() const
{
	return d->plugin.fileName();
}

bool QVstPlugin::load()
{
	if (d->plugin.fileName().isEmpty())
	{
		return false;
	}
	unload();
	d->plugin.load();
	vstFuncPtr mainEntryPoint = (vstFuncPtr)d->plugin.resolve("VSTPluginMain");
	if (mainEntryPoint == 0)
	{
		mainEntryPoint = (vstFuncPtr)d->plugin.resolve("main");
	}
	if (mainEntryPoint)
	{
		d->aeffect = mainEntryPoint(hostCallback);
	}
	if (!d->aeffect)
	{
		unload();
		return false;
	}
	d->ok = (d->aeffect->magic == kEffectMagic);
	if (!d->ok)
	{
		unload();
	}
	d->aeffect->user = d->edit_widget;
	ERect * r;
	d->aeffect->dispatcher(d->aeffect, effEditGetRect, 0, 0, (void **)& r, 0.0f);
	d->edit_widget->resize(r->right - r->left, r->bottom - r->top);
	d->edit_widget->move(r->left, r->top);
	d->aeffect->dispatcher(d->aeffect, effOpen, 0, 0, NULL, 0.0f);
	return d->ok;

}

bool QVstPlugin::unload()
{
	if (d->ok)
	{
		d->aeffect->dispatcher(d->aeffect, effClose, 0, 0, NULL, 0.0f);
	}

	d->aeffect = 0;
	d->ok = false;
	if (d->plugin.isLoaded())
	{
		return d->plugin.unload();
	}
	return false;
}

bool QVstPlugin::isLoaded() const
{
	return d->ok;
}

const AEffect * QVstPlugin::lowLevelApi() const
{
	return d->aeffect;
}

void QVstPlugin::resume()
{
	if (!d->ok)
	{
		return;
	}
	d->aeffect->dispatcher(d->aeffect, effMainsChanged, 0, 1, NULL, 0.0f);
	d->aeffect->dispatcher(d->aeffect, effStartProcess, 0, 0, NULL, 0.0f);
	d->suspended = false;
}

void QVstPlugin::suspend()
{
	if (!d->ok)
	{
		return;
	}
	d->aeffect->dispatcher(d->aeffect, effStopProcess, 0, 0, NULL, 0.0f);
	d->aeffect->dispatcher(d->aeffect, effMainsChanged, 0, 0, NULL, 0.0f);
	d->suspended = true;
}

bool QVstPlugin::isSuspended() const
{
	return d->suspended;
}

bool QVstPlugin::canDo(const QString & canDoString)
{
	if (!d->ok)
	{
		return false;
	}
	char str[1024];
	qstrcpy(str, canDoString.toLocal8Bit().constData());
	return (d->aeffect->dispatcher(d->aeffect, effCanDo, 0, 0, str, 0.0f) > 0);
}

void QVstPlugin::setSampleRate(float sr)
{
	if (!d->ok)
	{
		return;
	}
	d->aeffect->dispatcher(d->aeffect, effSetSampleRate, 0, 0, NULL,  d->samplerate = sr);
}

float QVstPlugin::sampleRate() const
{
	return d->samplerate;
}

void QVstPlugin::setBlockSize(int sz)
{
	if (!d->ok)
	{
		return;
	}
	d->aeffect->dispatcher(d->aeffect, effSetBlockSize, 0, d->blocksize = sz, NULL, 0.0f);
}

int QVstPlugin::blockSize() const
{
	return d->blocksize;
}

void QVstPlugin::setBypass(bool state)
{
	if (!d->ok)
	{
		return;
	}
	d->aeffect->dispatcher(d->aeffect, effSetBypass, 0, d->bypass = (state ? 1 : 0), NULL, 0.0f);
}

bool QVstPlugin::bypass() const
{
	return d->bypass;
}

QWidget * QVstPlugin::editWidget() const
{
	return d->edit_widget;
}

void QVstPlugin::editOpen()
{
	if (!d->ok)
	{
		return;
	}
	d->edit_widget->show();
	d->aeffect->dispatcher(d->aeffect, effEditOpen, 0, 0, (void*)d->edit_widget->winId(), 0.0f);
}

void QVstPlugin::editClose()
{
	if (!d->ok)
	{
		return;
	}
	d->edit_widget->hide();
	d->aeffect->dispatcher(d->aeffect, effEditClose, 0, 0, NULL, 0.0f);
}

int QVstPlugin::inputsCount() const
{
	if (!d->ok)
	{
		return 0;
	}
	return d->aeffect->numInputs;
}

int QVstPlugin::outputsCount() const
{
	if (!d->ok)
	{
		return 0;
	}
	return d->aeffect->numOutputs;
}

QList<VstPinProperties> QVstPlugin::inputs() const
{
	QList<VstPinProperties> l;
	for (int i = 0; i < inputsCount(); i++)
	{
		VstPinProperties p;
		qMemSet(& p, 0, sizeof(p));
		d->aeffect->dispatcher(d->aeffect, effGetInputProperties, 0, 0, & p, 0.0f);
		l << p;
	}
	return l;
}

QList<VstPinProperties> QVstPlugin::outputs() const
{
	QList<VstPinProperties> l;
	for (int i = 0; i < outputsCount(); i++)
	{
		VstPinProperties p;
		qMemSet(& p, 0, sizeof(p));
		d->aeffect->dispatcher(d->aeffect, effGetOutputProperties, 0, 0, & p, 0.0f);
		l << p;
	}
	return l;
}

bool QVstPlugin::isGenerator() const
{
	if (!d->ok)
	{
		return false;
	}
	return inputsCount() == 0;
}

int QVstPlugin::programsCount() const
{
	if (!d->ok)
	{
		return 0;
	}
	return d->aeffect->numPrograms;
}

void QVstPlugin::setProgram(int i, const QString * new_program_name)
{
	if (!d->ok)
	{
		return;
	}
	if (i < 0 || i >= programsCount())
	{
		return;
	}
	d->aeffect->dispatcher(d->aeffect, effBeginSetProgram, 0, 0, NULL, 0.0f);
	d->aeffect->dispatcher(d->aeffect, effSetProgram, 0, i, NULL, 0.0f);
	d->aeffect->dispatcher(d->aeffect, effEndSetProgram, 0, 0, NULL, 0.0f);
	if (new_program_name)
	{
		char name[kVstMaxProgNameLen + 1];
		qstrcpy(name, new_program_name->toLocal8Bit().constData());
		d->aeffect->dispatcher(d->aeffect, effSetProgramName, 0, i, NULL, 0.0f);
	}
}

int QVstPlugin::program(QString * program_name) const
{
	if (!d->ok)
	{
		return 0;
	}
	if (program_name)
	{
		char name[kVstMaxProgNameLen + 1];
		d->aeffect->dispatcher(d->aeffect, effSetProgramName, 0, 0, name, 0.0f);
		* program_name = name;
	}
	return d->aeffect->dispatcher(d->aeffect, effGetProgram, 0, 0, NULL, 0.0f);
}

QStringList QVstPlugin::programs() const
{
	QStringList l;
	for (int i = 0; i < programsCount(); i++)
	{
		char name[kVstMaxProgNameLen + 1];
		d->aeffect->dispatcher(d->aeffect, effGetProgramNameIndexed, 0, i, name, 0.0f);
		l << name;
	}
	return l;
}

void QVstPlugin::savePreset(QSettings & s) const
{
	if (!d->ok)
	{
		return;
	}
	s.beginGroup(QString::number(d->chainindex) + "_" + QString::number(id()));
	s.setValue("EffectName", effectName());
	s.setValue("Id", id());
	s.setValue("VstVersion", vstVersion());
	s.setValue("PluginVersion", pluginVersion());
	s.setValue("VendorVersion", vendorVersion());
	s.setValue("BlockSize", blockSize());
	s.setValue("SampleRate", QString::number(sampleRate()));
	for (int i = 0; i < parametersCount(); i++)
	{
		s.setValue(QString::number(i), QString::number(parameter(i)));
	}
	s.endGroup();
}

bool QVstPlugin::loadPreset(QSettings & s)
{
	if (!d->ok)
	{
		return false;
	}
	s.beginGroup(QString::number(d->chainindex) + "_" + QString::number(id()));
	const bool valid = s.value("Id").toInt() == id();
	s.endGroup();
	if (!valid)
	{
		return false;
	}
	s.beginGroup(QString::number(d->chainindex) + "_" + QString::number(id()));
	setBlockSize(s.value("BlockSize").toInt());
	setSampleRate(s.value("SampleRate").toFloat());
	for (int i = 0; i < parametersCount(); i++)
	{
		setParameter(i, s.value(QString::number(i)).toFloat());
	}
	s.endGroup();
	return true;
}

void QVstPlugin::savePreset(const QString & name) const
{
	if (!d->ok)
	{
		return;
	}
//	QSettings(name, QSettings::IniFormat).clear();
	savePreset(QSettings(name, QSettings::IniFormat));
}

bool QVstPlugin::loadPreset(const QString & name)
{
	return loadPreset(QSettings(name, QSettings::IniFormat));
}

int QVstPlugin::parametersCount() const
{
	if (!d->ok)
	{
		return 0;
	}
	return d->aeffect->numParams;
}

void QVstPlugin::setParameter(int i, float value)
{
	if (!d->ok)
	{
		return;
	}
	if (i < 0 || i >= parametersCount())
	{
		return;
	}
	d->aeffect->setParameter(d->aeffect, i, value);
}

float QVstPlugin::parameter(int i, VstParameterProperties * properties) const
{
	if (!d->ok)
	{
		return 0.0f;
	}
	if (i < 0 || i >= parametersCount())
	{
		return 0.0f;
	}
	if (properties)
	{
		qMemSet(properties, 0, sizeof(* properties));
		d->aeffect->dispatcher(d->aeffect, effGetParameterProperties, i, 0, properties, 0.0f);
	}
	return d->aeffect->getParameter(d->aeffect, i);
}

void QVstPlugin::setParameters(const QList<float> & l)
{
	for (int i = 0; i < qMin(parametersCount(), l.count()); i++)
	{
		setParameter(i, l[i]);
	}
}

QList<float> QVstPlugin::parameters(QList<VstParameterProperties> * properties) const
{
	QList<float> l;
	for (int i = 0; i < parametersCount(); i++)
	{
		VstParameterProperties p;
		const float v = parameter(i, & p);
		if (properties)
		{
			* properties << p;
		}
		l << v;
	}
	return l;
}

int QVstPlugin::id() const
{
	if (!d->ok)
	{
		return 0;
	}
	return d->aeffect->uniqueID;
}

int QVstPlugin::pluginVersion() const
{
	if (!d->ok)
	{
		return 0;
	}
	return d->aeffect->version;
}

int QVstPlugin::vstVersion() const
{
	if (!d->ok)
	{
		return 0;
	}
	return d->aeffect->dispatcher(d->aeffect, effGetVstVersion, 0, 0, NULL, 0.0f);
}

int QVstPlugin::vendorVersion() const
{
	if (!d->ok)
	{
		return 0;
	}
	return d->aeffect->dispatcher(d->aeffect, effGetVendorVersion, 0, 0, NULL, 0.0f);
}

QString QVstPlugin::effectName() const
{
	if (!d->ok)
	{
		return 0;
	}
	char name[kVstMaxEffectNameLen + 1];
	name[0] = '\0';
	d->aeffect->dispatcher(d->aeffect, effGetEffectName, 0, 0, name, 0.0f);
	return name;
}

VstPlugCategory QVstPlugin::category() const
{
	if (!d->ok)
	{
		return kPlugCategUnknown;
	}
	return (VstPlugCategory)d->aeffect->dispatcher(d->aeffect, effGetPlugCategory, 0, 0, NULL, 0.0f);
}

bool QVstPlugin::canProcessFloat() const
{
	if (!d->ok)
	{
		return false;
	}
	return (d->aeffect->flags & effFlagsCanReplacing) == effFlagsCanReplacing;
}

bool QVstPlugin::canProcessDouble() const
{
	if (!d->ok)
	{
		return false;
	}
	return (d->aeffect->flags & effFlagsCanDoubleReplacing) == effFlagsCanDoubleReplacing;
}

bool QVstPlugin::process(const float ** input, float ** output, int count)
{
	if (!d->ok)
	{
		return false;
	}
	if (!canProcessFloat())
	{
		return false;
	}
	d->aeffect->dispatcher(d->aeffect, effSetProcessPrecision, 0, kVstProcessPrecision32, NULL, 0.0f);

	const float ** tmp_input = d->aeffect->numInputs > 0 ? new const float * [d->aeffect->numInputs] : 0;
	float ** tmp_output = d->aeffect->numOutputs > 0 ? new float * [d->aeffect->numOutputs] : 0;
	for (int offset = 0; offset < count; offset += d->blocksize)
	{
		for (int i = 0; i < d->aeffect->numInputs; i++)
		{
			tmp_input[i] = input[i] + offset;
		}
		for (int i = 0; i < d->aeffect->numOutputs; i++)
		{
			tmp_output[i] = output[i] + offset;
		}
		d->aeffect->processReplacing(d->aeffect, (float **)tmp_input, tmp_output, qMin(d->blocksize, count - offset));
	}
	if (d->aeffect->numInputs)
	{
		delete [] tmp_input;
	}
	if (d->aeffect->numOutputs)
	{
		delete [] tmp_output;
	}
	return true;
}

bool QVstPlugin::process(const double ** input, double ** output, int count)
{
	if (!d->ok)
	{
		return false;
	}
	if (!canProcessDouble())
	{
		return false;
	}
	d->aeffect->dispatcher(d->aeffect, effSetProcessPrecision, 0, kVstProcessPrecision32, NULL, 0.0f);

	const double ** tmp_input = d->aeffect->numInputs > 0 ? new const double * [d->aeffect->numInputs] : 0;
	double ** tmp_output = d->aeffect->numOutputs > 0 ? new double * [d->aeffect->numOutputs] : 0;
	for (int offset = 0; offset < count; offset += d->blocksize)
	{
		for (int i = 0; i < d->aeffect->numInputs; i++)
		{
			tmp_input[i] = input[i] + offset;
		}
		for (int i = 0; i < d->aeffect->numOutputs; i++)
		{
			tmp_output[i] = output[i] + offset;
		}
		d->aeffect->processDoubleReplacing(d->aeffect, (double **)tmp_input, tmp_output, qMin(d->blocksize, count - offset));
	}
	if (d->aeffect->numInputs)
	{
		delete [] tmp_input;
	}
	if (d->aeffect->numOutputs)
	{
		delete [] tmp_output;
	}
	return true;
}

QList< QVector<float> > QVstPlugin::process(const QList< QVector<float> > & in)
{
	QList< QVector<float> > out;
	if (in.count() != d->aeffect->numInputs || !canProcessFloat())
	{
		return out;
	}
	const float ** inputs = d->aeffect->numInputs > 0 ? new const float * [d->aeffect->numInputs] : 0;
	float ** outputs = d->aeffect->numOutputs > 0 ? new float * [d->aeffect->numOutputs] : 0;
	int count = 0;
	for (int i = 0; i < d->aeffect->numInputs; i++)
	{
		inputs[i] = (float *)in[i].data();
		if (count == 0 || in[i].count() < count)
		{
			count = in[i].count();
		}
	}
	if (d->aeffect->numInputs == 0)
	{
		count = d->blocksize;
	}
	if (count > 0)
	{
		for (int i = 0; i < d->aeffect->numOutputs; i++)
		{
			out << QVector<float>(count);
			outputs[i] = (float *)out[i].data();
		}
		process(inputs, outputs, count);
	}
	if (d->aeffect->numInputs)
	{
		delete [] inputs;
	}
	if (d->aeffect->numOutputs)
	{
		delete [] outputs;
	}
	return out;
}

QList< QVector<double> > QVstPlugin::process(const QList< QVector<double> > & in)
{
	QList< QVector<double> > out;
	if (in.count() != d->aeffect->numInputs || !canProcessDouble())
	{
		return out;
	}
	const double ** inputs = d->aeffect->numInputs > 0 ? new const double * [d->aeffect->numInputs] : 0;
	double ** outputs = d->aeffect->numOutputs > 0 ? new double * [d->aeffect->numOutputs] : 0;
	int count = 0;
	for (int i = 0; i < d->aeffect->numInputs; i++)
	{
		inputs[i] = (double *)in[i].data();
		if (count == 0 || in[i].count() < count)
		{
			count = in[i].count();
		}
	}
	if (d->aeffect->numInputs == 0)
	{
		count = d->blocksize;
	}
	if (count > 0)
	{
		for (int i = 0; i < d->aeffect->numOutputs; i++)
		{
			out << QVector<double>(count);
			outputs[i] = (double *)out[i].data();
		}
		process(inputs, outputs, count);
	}
	if (d->aeffect->numInputs)
	{
		delete [] inputs;
	}
	if (d->aeffect->numOutputs)
	{
		delete [] outputs;
	}
	return out;
}

QVector<float> QVstPlugin::processOne(const QVector<float> & in)
{
	QList< QVector<float> > in_list;
	for (int i = 0; i < d->aeffect->numInputs; i++)
	{
		in_list << in;
	}
	const QList< QVector<float> > & out_list = process(in_list);
	if (out_list.isEmpty())
	{
		return QVector<float>();
	}
	return out_list.front();
}

QVector<double> QVstPlugin::processOne(const QVector<double> & in)
{
	QList< QVector<double> > in_list;
	for (int i = 0; i < d->aeffect->numInputs; i++)
	{
		in_list << in;
	}
	const QList< QVector<double> > & out_list = process(in_list);
	if (out_list.isEmpty())
	{
		return QVector<double>();
	}
	return out_list.front();
}

// ---------------------------------------------------------------------------------


struct QVstChain::Data
{
	Data()
	{
	}
	~Data()
	{
	}
};

QVstChain::QVstChain(): QList<QVstPlugin>(), d(new Data())
{
}

QVstChain::QVstChain(const QString & preset): QList<QVstPlugin>(), d(new Data())
{
	loadPreset(preset);
}

QVstChain::QVstChain(const QStringList & names): QList<QVstPlugin>(), d(new Data())
{
	load(names);
}

QVstChain::QVstChain(const QVstChain & o): QList<QVstPlugin>(o), d(new Data())
{
	* d = * o.d;
}

QVstChain & QVstChain::operator = (const QVstChain & o)
{
	if (& o != this)
	{
		* d = * o.d;
	}
	return * this;
}

QVstChain::~QVstChain()
{
	unload();
	delete d;
}

bool QVstChain::load(const QString & name)
{
	return load(QStringList() << name);
}

bool QVstChain::load(const QStringList & names)
{
	clear();
	foreach (const QString name, names)
	{
		QVstPlugin vst;
		if (!vst.load(name))
		{
			clear();
			return false;
		}
		append(vst);
	}
	return true;
}

bool QVstChain::unload()
{
	for (QVstChain::iterator i = begin(); i != end(); i++)
	{
		if (!i->unload())
		{
			return false;
		}
	}
	return true;
}

void QVstChain::resume()
{
	for (QVstChain::iterator i = begin(); i != end(); i++)
	{
		i->resume();
	}
}

void QVstChain::suspend()
{
	for (QVstChain::iterator i = begin(); i != end(); i++)
	{
		i->suspend();
	}
}

void QVstChain::setBypass(bool b)
{
	for (QVstChain::iterator i = begin(); i != end(); i++)
	{
		i->setBypass(b);
	}
}

void QVstChain::setSampleRate(float sr)
{
	for (QVstChain::iterator i = begin(); i != end(); i++)
	{
		i->setSampleRate(sr);
	}
}

void QVstChain::setBlockSize(int sz)
{
	for (QVstChain::iterator i = begin(); i != end(); i++)
	{
		i->setBlockSize(sz);
	}
}

QWidgetList QVstChain::editWidgets() const
{
	QWidgetList w;
	foreach (const QVstPlugin & vst, * this)
	{
		w << vst.editWidget();
	}
	return w;
}

int QVstChain::inputsCount() const
{
	if (isEmpty())
	{
		return 0;
	}
	int count = 0x7fffffff;
	foreach(const QVstPlugin & vst, * this)
	{
		const int inputs = vst.inputsCount();
		if (inputs < count)
		{
			count = inputs;
		}
	}
	return count;
}

int QVstChain::outputsCount() const
{
	if (isEmpty())
	{
		return 0;
	}
	int count = 0x7fffffff;
	foreach(const QVstPlugin & vst, * this)
	{
		const int outputs = vst.outputsCount();
		if (outputs < count)
		{
			count = outputs;
		}
	}
	return count;
}

void QVstChain::savePreset(const QString & name)
{
	QSettings s(name, QSettings::IniFormat);
	s.clear();
	int k = 0;
	s.beginGroup("Chain");
	for (QVstChain::iterator i = begin(); i != end(); i++, k++)
	{
		i->d->chainindex = k;
		s.setValue(QString::number(k), i->vstFileName());
	}
	s.endGroup();
	foreach(const QVstPlugin & vst, * this)
	{
		vst.savePreset(s);
	}
}

bool QVstChain::loadPreset(const QString & name)
{
	clear();
	QSettings s(name, QSettings::IniFormat);
	s.beginGroup("Chain");
	foreach (const QString & key, s.childKeys())
	{
		QVstPlugin vst;
		if (!vst.load(s.value(key).toString()))
		{
			s.endGroup();
			return false;
		}
		vst.d->chainindex = key.toInt();
		while (count() <= vst.d->chainindex)
		{
			append(QVstPlugin());
		}
		replace(vst.d->chainindex, vst); 
	}
	s.endGroup();

	for (QVstChain::iterator i = begin(); i != end(); i++)
	{
		if (!i->loadPreset(s))
		{
			return false;
		}
	}
	return true;
}

bool QVstChain::canProcessFloat() const
{
	if (isEmpty())
	{
		return false;
	}
	foreach(const QVstPlugin & vst, * this)
	{
		if (!vst.canProcessFloat())
		{
			return false;
		}
	}
	return true;
}

bool QVstChain::canProcessDouble() const
{
	if (isEmpty())
	{
		return false;
	}
	foreach(const QVstPlugin & vst, * this)
	{
		if (!vst.canProcessDouble())
		{
			return false;
		}
	}
	return true;
}

int QVstChain::linksCount() const
{
	int count = 0x7fffffff;
	for (QVstChain::const_iterator i = begin(); i != end(); i++)
	{
		const int inputs = i->inputsCount();
		if (i != begin() && inputs < count)
		{
			count = inputs;
		}
		const int outputs = i->outputsCount();
		if (outputs < count)
		{
			count = outputs;
		}
	}
	if (count == 0x7fffffff)
	{
		count = 0;
	}
	return count;
}

bool QVstChain::isGenerator() const
{
	if (isEmpty())
	{
		return false;
	}
	return front().isGenerator();
}

bool QVstChain::canProcess(int i) const
{
	if (i < 0)
	{
		return false;
	}
	if (i == 0)
	{
		return isGenerator();
	}
	return (i <= linksCount());
}

QList< QVector<float> > QVstChain::process(const QList< QVector<float> > & in)
{
	QList< QVector<float> > out;
	if (!canProcessFloat())
	{
		return out;
	}
	if (canProcess(in.count()))
	{
		out = in;
		for (QVstChain::iterator i = begin(); i != end(); i++)
		{
			while (out.count() < i->inputsCount())
			{
				out.append(out[out.count() - 1]);
			}
			out = i->process(out.mid(0, i->inputsCount()));
		}		
	}
	return out.mid(0, in.count());
}

QList< QVector<double> > QVstChain::process(const QList< QVector<double> > & in)
{
	QList< QVector<double> > out;
	if (!canProcessFloat())
	{
		return out;
	}
	if (canProcess(in.count()))
	{
		out = in;
		for (QVstChain::iterator i = begin(); i != end(); i++)
		{
			while (out.count() < i->inputsCount())
			{
				out.append(out[out.count() - 1]);
			}
			out = i->process(out.mid(0, i->inputsCount()));
		}		
	}
	return out.mid(0, in.count());
}

QVector<float> QVstChain::processOne(const QVector<float> & in)
{
	const QList< QVector<float> > & out = process(QList< QVector<float> >() << in);
	if (out.isEmpty())
	{
		return QVector<float>();
	}
	return out.front();
}

QVector<double> QVstChain::processOne(const QVector<double> & in)
{
	const QList< QVector<double> > & out = process(QList< QVector<double> >() << in);
	if (out.isEmpty())
	{
		return QVector<double>();
	}
	return out.front();
}

