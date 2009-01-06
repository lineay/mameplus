#ifndef _MAMEOPT_H_
#define _MAMEOPT_H_

#include <QtGui>

enum
{
	OPTLEVEL_DEF = 0,
	OPTLEVEL_GLOBAL,
	OPTLEVEL_SRC,
	OPTLEVEL_BIOS,
	OPTLEVEL_CLONEOF,
	OPTLEVEL_CURR,
	OPTLEVEL_LAST
};

class ResetWidget : public QWidget
{
	Q_OBJECT
public:
	QWidget *subWidget;
	QWidget *subWidget2;

	ResetWidget(/*QtProperty *property,*/ QWidget *parent = 0);

	void setWidget(QWidget *, QWidget * = NULL, int = 0, int = 0);
	void setResetEnabled(bool enabled);
/*	void setValueText(const QString &text);
	void setValueIcon(const QIcon &icon);*/
	void setSpacing(int spacing);

signals:
//	void resetProperty(QtProperty *property);
private slots:
	void slotClicked();

public slots:
	void updateSliderLabel(int);


private:
//	QtProperty *m_property;
	QLabel *_textLabel;
	QLabel *_iconLabel;
	QSlider *_slider;
	QLabel *_sliderLabel;
	QToolButton *_btnFileDlg;
	QToolButton *_btnReset;
	int ctrlSpacing;
	int optType;
	int sliderOffset;
};


class OptionDelegate : public QItemDelegate
{
	Q_OBJECT

public:
	OptionDelegate(QObject *parent = 0);

	QSize sizeHint ( const QStyleOptionViewItem & option, 
		const QModelIndex & index ) const;
	void paint(QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index ) const;
	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
		const QModelIndex &index) const;
	void setEditorData(QWidget *editor, const QModelIndex &index) const;
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
	void updateEditorGeometry(QWidget *editor,
		const QStyleOptionViewItem &option, const QModelIndex &index) const;

public slots:
	void sync();
	void setDirectories();
	void setDirectoriesAccepted();
	void setDirectory();
	void setFile(QString = "", ResetWidget* = NULL);
	void setDatFile();
	void setExeFile();
	void setCfgFile();

private:
	ResetWidget *rWidget;
	bool isReset;
	QString pathBuf;
};

class MameOption : public QObject
{
public:
	QString guiname, defvalue, description, currvalue, max, min, 
		globalvalue, srcvalue, biosvalue, cloneofvalue;
	
	int type;
	bool guivisible, globalvisible, srcvisible, biosvisible, cloneofvisible, gamevisible;
	QStringList values;
	QStringList guivalues;

	MameOption(QObject *parent = 0);
};

class OptInfo : public QObject
{
public:
	QListWidget *lstCatView;
	QTreeView *optView;
	QStandardItemModel *optModel;

	OptInfo(QListWidget *, QTreeView *, QObject *parent = 0);
};

class OptionUtils : public QObject
{
	Q_OBJECT

public:
	OptionUtils(QObject *parent = 0);
	void initOption();
	QVariant getField(const QModelIndex &, int);
	const QString getLongName(QString);
	const QString getLongValue(const QString &, const QString &);
	const QString getShortValue(const QString &, const QString &);
	QColor inheritColor(const QModelIndex &);
	bool isChanged(const QModelIndex &);
	bool isTitle(const QModelIndex &);
	void updateSelectableItems(QString);
	void saveIniFile(int , const QString &);

public slots:
	void loadDefault(QString);
	void loadTemplate();
	void loadIni(int, const QString &);
	void updateModel(QListWidgetItem *currItem = 0, int optType = -1);
	void updateHeaderSize(int, int, int);

private:
	//option category map, option category as key, names as value list
	QMap<QString, QStringList> optCatMap;
	QList<OptInfo *> optInfos;

	QHash<QString, QString> readIniFile(const QString &);
	void addModelItemTitle(QStandardItemModel*, QString);
	void addModelItem(QStandardItemModel*, QString);
	void updateModelData(QString, int);
};

extern OptionUtils *optUtils;
extern QHash<QString, MameOption*> mameOpts;
extern QByteArray option_column_state;
extern QString mameIniPath;

#endif
