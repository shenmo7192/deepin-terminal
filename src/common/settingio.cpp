#include "settingio.h"

#include <QTextStream>
#include <qdatastream.h>
#include <QDebug>
#include <QVariant>
#include <QTextCodec>

#define SLASH_REPLACE_CHAR QChar(0x01)
bool SettingIO::rewrite = false;
SettingIO::SettingIO(QObject *parent) : QObject(parent)
{

}
/*******************************************************************************
 1. @函数:    readIniFunc
 2. @作者:    ut001811 朱科伟
 3. @日期:    2020-08-31
 4. @说明: 读取自定义配置文件
*******************************************************************************/
bool SettingIO::readIniFunc(QIODevice &device, QSettings::SettingsMap &settingsMap)
{
    QString currentSection;
       QTextStream stream(&device);
       stream.setCodec("UTF-8");
       QString data;
       //bool ok = true;
       rewrite = false;
       while (!stream.atEnd()) {
           data = stream.readLine();
           if (data.trimmed().isEmpty()) {
               continue;
           }
           if (data[0] == QChar('[')) {
               QString iniSection;
               int inx = data.indexOf(QChar(']'));
               if (inx == -1){
                   //ok = false;
                   iniSection = data.mid(1);
               } else {
                   iniSection = data.mid(1, inx - 1);
               }

               iniSection = iniSection.trimmed();
               if (iniSection.compare(QString("general"), Qt::CaseInsensitive) == 0) {
                   currentSection.clear();
               } else {
                   if (iniSection.compare(QString("%general"), Qt::CaseInsensitive) == 0) {
                       currentSection = QString("general");
                   } else {
                      iniSection = canTransfer(iniSection);
                       currentSection = iniSection;
                   }
                   currentSection.replace('/', SLASH_REPLACE_CHAR);
                   currentSection += QChar('/');
               }
           } else {
               bool inQuotes = false;
               int equalsPos = -1;
               QList<int> commaPos;
               int charPos = 0;
               while (charPos < data.size())
               {
                   QChar ch = data.at(charPos);
                   if (ch == QChar('=')) {
                       if (!inQuotes && equalsPos == -1) {
                           equalsPos = charPos;
                       }
                   } else if (ch == QChar('"')) {
                       inQuotes = !inQuotes;
                   } /*else if (ch == QChar(',')) {

                       if (!inQuotes && equalsPos != -1) {
                           commaPos.append(i);
                       }

                   }*/ /*else if (ch == QChar(';') || ch == QChar('#')) {
                       if (!inQuotes) {
                           data.resize(i);
                           break;
                       }
                   }*/ /*else if (ch == QChar('\\')) {
                       if (++i < data.size()) {
                        } else {
                           ok = false;
                           break;
                       }
                   }
                   */
                   charPos++;
               }
                if (equalsPos == -1) {
                   break;
               } else {
                   QString key = data.mid(0, equalsPos).trimmed();
                   if (key.isEmpty()) {
                       break;
                   } else {
                       key = currentSection + key;
                   }
                   if (commaPos.isEmpty()) { //value
                       QString v = data.mid(equalsPos+1).trimmed();
                       if (v.startsWith("\"") && v.endsWith("\"") && v.length()>1) {
                           v = v.mid(1, v.length()-2);
                       }
                        QString str = unescapedString(v);
                        //如果是3字节的Latin1中文，转换中文编码
                        if(v.count("\\x") > 0 && v.count("\\x") %3 == 0)
                        {
                            int first = v.indexOf("\\x");
                            int next = v.indexOf("\\x",first+1);
                            int len = next-first;
                            if(len == 4)
                            {
                                str = QString::fromLocal8Bit(str.toLatin1());
                            }
                        }

                        settingsMap[key] = stringToVariant(str);
                   } else { //value list
                       commaPos.prepend(equalsPos);
                       commaPos.append(-1);
                       QVariantList vals;
                       for (int pos=1; pos<commaPos.size(); ++pos) {
                           QString d = data.mid(commaPos.at(pos-1)+1, commaPos.at(pos)-commaPos.at(pos-1)-1);
                           QString v = d.trimmed();
                           if (v.startsWith("\"") && v.endsWith("\"") && v.length()>1) {
                               v = v.mid(1, v.length()-2);
                           }
                           vals.append(stringToVariant(unescapedString(v)));
                       }
                       settingsMap[key] = vals;
                   }
               }
           }
       }
       return true;
      //return ok;
}
/*******************************************************************************
 1. @函数:    writeIniFunc
 2. @作者:    ut001811 朱科伟
 3. @日期:    2020-08-31
 4. @说明: 写入自定义配置文件
*******************************************************************************/
bool SettingIO::writeIniFunc(QIODevice &device, const QSettings::SettingsMap &settingsMap)
{
#ifdef Q_OS_WIN
    const char * const eol = "\r\n";
#else
    const char* eol = "\n";
#endif
    bool writeError = false;

    QString lastSection;
    QMapIterator<QString,QVariant> it(settingsMap);
    while(it.hasNext() && !writeError) {
      it.next();
      QString key = it.key();
      QString section;
     // qDebug()<<"key: "<<key;
      int idx = key.lastIndexOf(QChar('/'));
      if (idx == -1) {
          section = QString("[General]");
      } else {
          section = key.left(idx);
          key = key.mid(idx+1);
          if (section.compare(QString("General"), Qt::CaseInsensitive) == 0) {
              section = QString("[%General]");
          } else {
              section.prepend(QChar('['));
              section.append(QChar(']'));
          }

          section.replace(SLASH_REPLACE_CHAR, '/');
      }
      if (section.compare(lastSection, Qt::CaseInsensitive))
      {
          if (!lastSection.isEmpty()) {
              device.write(eol);
          }
          lastSection = section;
          if (device.write(section.toUtf8() + eol) == -1) {
              writeError = true;
          }
      }
       QByteArray block = key.toUtf8();
      block += " = ";
      if (it.value().type() == QVariant::StringList) {
          foreach (QString s, it.value().toStringList()) {
              block += escapedString(s);
              block += ", ";
          }
          if (block.endsWith(", ")) {
              block.chop(2);
          }
       } else if (it.value().type() == QVariant::List) {
          foreach (QVariant v, it.value().toList()) {
              block += escapedString(variantToString(v));
              block += ", ";
          }
          if (block.endsWith(", ")) {
              block.chop(2);
          }
      } else {
          block += escapedString(variantToString(it.value()));
      }
      block += eol;
      if (device.write(block) == -1) {
          writeError = true;
      }
     }
    return true;
    //return writeError;
}

/*******************************************************************************
 1. @函数:    variantToString
 2. @作者:    ut001811 朱科伟
 3. @日期:    2020-08-31
 4. @说明: variant 在转换成QString
*******************************************************************************/
QString SettingIO::variantToString(const QVariant &v)
{
    QString result;
    switch (v.type()) {
        case QVariant::String:
        case QVariant::LongLong:
        case QVariant::ULongLong:
        case QVariant::Int:
        case QVariant::UInt:
        case QVariant::Bool:
        case QVariant::Double:
        case QVariant::KeySequence: {
            result = v.toString();
            if (result.startsWith(QChar('@')))
                result.prepend(QChar('@'));
            break;
        }
        default: {
            QByteArray a;
            {
                QDataStream s(&a, QIODevice::WriteOnly);
                s.setVersion(QDataStream::Qt_4_0);
                s << v;
            }

            result = QString("@Variant(");
            result += QString::fromLatin1(a.constData(), a.size());
            result += QChar(')');
            break;
        }
    }

    return result;
}

/*******************************************************************************
 1. @函数:    stringToVariant
 2. @作者:    ut001811 朱科伟
 3. @日期:    2020-08-31
 4. @说明: 将QString转换为Variant
*******************************************************************************/
QVariant SettingIO::stringToVariant(const QString &s)
{
    if (s.startsWith(QChar('@'))) {
        if (s.endsWith(QChar(')'))) {
            if (s.startsWith(QString("@Variant("))) {
                QByteArray a(s.toUtf8().mid(9));
                QDataStream stream(&a, QIODevice::ReadOnly);
                stream.setVersion(QDataStream::Qt_4_0);
                QVariant result;
                stream >> result;
                return result;
            }
        }
        if (s.startsWith(QString("@@")))
            return QVariant(s.mid(1));
    }
    return QVariant(s);
}

/*******************************************************************************
 1. @函数:    escapedString
 2. @作者:    ut001811 朱科伟
 3. @日期:    2020-08-31
 4. @说明:   加入特殊字符\x
*******************************************************************************/
QByteArray SettingIO::escapedString(const QString &src)
{
    bool needsQuotes = false;
    bool escapeNextIfDigit = false;
    int i;
    QByteArray result;
    result.reserve(src.size() * 3 / 2);
    for (i = 0; i < src.size(); ++i) {
        uint ch = src.at(i).unicode();
        if (ch == ';' || ch == ',' || ch == '=' || ch == '#') {
            needsQuotes = true;
        }
        if (escapeNextIfDigit && ((ch >= '0' && ch <= '9')
                 || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) {
            result += "\\x";
            result += QByteArray::number(ch, 16);
            continue;
        }

        escapeNextIfDigit = false;

        switch (ch) {
        case '\0':
            result += "\\0";
            escapeNextIfDigit = true;
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        case '"':
        case '\\':
            result += '\\';
            result += (char)ch;
            break;
        default:
            if (ch <= 0x1F) {
                result += "\\x";
                result += QByteArray::number(ch, 16);
                escapeNextIfDigit = true;
            } else{
                result += QString(src[i]).toUtf8();
            }
        }
    }
    if (result.size()>0 && (result.at(0)==' ' || result.at(result.size() - 1) == ' ')) {
        needsQuotes = true;
    }
    if (needsQuotes) {
        result.prepend('"');
        result.append('"');
    }
    return result;
}


const char hexDigits[] = "0123456789ABCDEF";
/*******************************************************************************
 1. @函数:    unescapedString
 2. @作者:    ut001811 朱科伟
 3. @日期:    2020-08-31
 4. @说明:    去除特殊字符\x
*******************************************************************************/
QString SettingIO::unescapedString(const QString &src)
{
    static const char escapeCodes[][2] =
    {
        { 'a', '\a' },
        { 'b', '\b' },
        { 'f', '\f' },
        { 'n', '\n' },
        { 'r', '\r' },
        { 't', '\t' },
        { 'v', '\v' },
        { '"',  '"' },
        { 's',   ' '},
        { '?',  '?' },
        { '\'', '\'' },
        { '\\', '\\' }

    };
    static const int numEscapeCodes = sizeof(escapeCodes) / sizeof(escapeCodes[0]);

    QString stringResult;
    int escapeVal = 0;
    QChar ch;
    int i = 0;
normal:
    while (i < src.size()) {
        ch = src.at(i);
        if (ch == QChar('\\')) {
            ++i;
            if (i >= src.size()) {
                break;
            }
            ch = src.at(i++);
            for (int j = 0; j < numEscapeCodes; ++j) {
                if (ch == escapeCodes[j][0]) {
                    stringResult += QChar(escapeCodes[j][1]);
                    goto normal;
                }
            }
            if (ch == 'x') {
                escapeVal = 0;
                if (i >= src.size())
                    break;
                ch = src.at(i);
                if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'))
                    goto hexEscape;
            } else if (ch >= '0' && ch <= '7') {
                escapeVal = ch.unicode() - '0';
                goto octEscape;
            } else {
                //skip
            }
        } else {
            stringResult += ch;
        }
                i++;
    }
    goto end;

hexEscape:
    if (i >= src.size()) {
        stringResult += QChar(escapeVal);
        goto end;
    }

    ch = src.at(i);
    if (ch >= 'a')
        ch = ch.unicode() - ('a' - 'A');
    if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')) {
        escapeVal <<= 4;
        escapeVal += strchr(hexDigits, ch.toLatin1()) - hexDigits;
        ++i;
        goto hexEscape;
    } else {
        stringResult += QChar(escapeVal);
        goto normal;
    }

octEscape:
    if (i >= src.size()) {
        stringResult += QChar(escapeVal);
        goto end;
    }

    ch = src.at(i);
    if (ch >= '0' && ch <= '7') {
        escapeVal <<= 3;
        escapeVal += ch.toLatin1() - '0';
        ++i;
        goto octEscape;
    } else {
        stringResult += QChar(escapeVal);
        goto normal;
    }

end:
    return stringResult;
}
/*******************************************************************************
 1. @函数:    canTransfer
 2. @作者:    ut001811 朱科伟
 3. @日期:    2020-08-31
 4. @说明:   uincode是否需要被转换
*******************************************************************************/
QString SettingIO::canTransfer(const QString &str)
{
    QString res;
    //如果有%U，是Uincode字符串
    if(str.contains("%U") || str.contains("%u"))
    {
        rewrite = true;
        //uincode
        iniUnescapedKey(str.toLocal8Bit(),0,str.size(),res);
      //  qDebug()<< "uincode" <<ok << res;
    }
    //如果是%是Latin1格式的字符串
    else if(str.contains("%"))
    {
        rewrite = true;
        //utf-8转换为uincode过了
        iniUnescapedKey(str.toLocal8Bit(),0,str.size(),res);
      //  qDebug()<< "utf-8 to uincode" <<ok << res;
       // qDebug() << QString::fromLocal8Bit(res.toLatin1());
        res = QString::fromLocal8Bit(res.toLatin1());
    }
    else
    {
        res = str;
    }

    return res;
}

/*******************************************************************************
 1. @函数:    iniUnescapedKey
 2. @作者:    ut001811 朱科伟
 3. @日期:    2020-08-31
 4. @说明:    去除unicode的标识信息 %U
*******************************************************************************/
bool SettingIO::iniUnescapedKey(const QByteArray &key, int from, int to, QString &result)
{
    bool lowercaseOnly = true;
    int i = from;
    result.reserve(result.length() + (to - from));
    while (i < to) {
        int ch = (uchar)key.at(i);

        if (ch == '\\') {
            result += QLatin1Char('/');
            ++i;
            continue;
        }

        if (ch != '%' || i == to - 1) {
            if (uint(ch - 'A') <= 'Z' - 'A') // only for ASCII
                lowercaseOnly = false;
            result += QLatin1Char(ch);
            ++i;
            continue;
        }

        int numDigits = 2;
        int firstDigitPos = i + 1;

        ch = key.at(i + 1);
        if (ch == 'U') {
            ++firstDigitPos;
            numDigits = 4;
        }

        if (firstDigitPos + numDigits > to) {
            result += QLatin1Char('%');
            // ### missing U
            ++i;
            continue;
        }

        bool ok;
        ch = key.mid(firstDigitPos, numDigits).toInt(&ok, 16);
        if (!ok) {
            result += QLatin1Char('%');
            // ### missing U
            ++i;
            continue;
        }

        QChar qch(ch);
        if (qch.isUpper())
            lowercaseOnly = false;
        result += qch;
        i = firstDigitPos + numDigits;
    }
    return lowercaseOnly;
}

//自定义规则
QSettings::Format USettings::g_customFormat = QSettings::registerFormat("conf", SettingIO::readIniFunc, SettingIO::writeIniFunc);

//构造函数
USettings::USettings(const QString &fileName, QObject *parent)
    : QSettings (fileName, g_customFormat, parent)
{
    QSettings::setIniCodec(QTextCodec::codecForName("UTF-8"));
}

//析构函数
USettings::~USettings(){}

/*******************************************************************************
 1. @函数:    beginGroup
 2. @作者:    ut001121 zhangmeng
 3. @日期:    2020-12-21
 4. @说明:    Appends prefix to the current group.
*******************************************************************************/
void USettings::beginGroup(const QString &prefix)
{
    QString tempPrefix = prefix;
    tempPrefix.replace('/', SLASH_REPLACE_CHAR);
    QSettings::beginGroup(tempPrefix);
}

/*******************************************************************************
 1. @函数:    endGroup
 2. @作者:    ut001121 zhangmeng
 3. @日期:    2020-12-21
 4. @说明:    Resets the group to what it was before the corresponding beginGroup() call.
*******************************************************************************/
void USettings::endGroup()
{
    QSettings::endGroup();
}

/*******************************************************************************
 1. @函数:    setValue
 2. @作者:    ut001121 zhangmeng
 3. @日期:    2020-12-21
 4. @说明:    Sets the value of setting key to value. If the key already exists, the previous value is overwritten.
*******************************************************************************/
void USettings::setValue(const QString &key, const QVariant &value)
{
    QString tempKey = key;
    tempKey.replace('/', SLASH_REPLACE_CHAR);
    QSettings::setValue(tempKey, value);
}

/*******************************************************************************
 1. @函数:    value
 2. @作者:    ut001121 zhangmeng
 3. @日期:    2020-12-21
 4. @说明:    Returns the value for setting key. If the setting doesn't exist, returns defaultValue.
*******************************************************************************/
QVariant USettings::value(const QString &key, const QVariant &defaultValue) const
{
    QString tempKey = key;
    tempKey.replace('/', SLASH_REPLACE_CHAR);
    QVariant value = QSettings::value(tempKey, defaultValue);
    return value.toString().replace(SLASH_REPLACE_CHAR, '/');
}

/*******************************************************************************
 1. @函数:    remove
 2. @作者:    ut001121 zhangmeng
 3. @日期:    2020-12-21
 4. @说明:    Removes the setting key and any sub-settings of key.
*******************************************************************************/
void USettings::remove(const QString &key)
{
    QString tempKey = key;
    tempKey.replace('/', SLASH_REPLACE_CHAR);
    QSettings::remove(tempKey);
}

/*******************************************************************************
 1. @函数:    contains
 2. @作者:    ut001121 zhangmeng
 3. @日期:    2020-12-21
 4. @说明:    Returns true if there exists a setting called key; returns false otherwise.
*******************************************************************************/
bool USettings::contains(const QString &key) const
{
    QString tempKey = key;
    tempKey.replace('/', SLASH_REPLACE_CHAR);
    return QSettings::contains(tempKey);
}

/*******************************************************************************
 1. @函数:    contains
 2. @作者:    ut001121 zhangmeng
 3. @日期:    2020-12-21
 4. @说明:    Returns a list of all key top-level groups that contain keys that can be read using the QSettings object.
*******************************************************************************/
QStringList USettings::childGroups()
{
    QStringList childGroups = QSettings::childGroups();
    for (QString &child : childGroups) {
        child.replace(SLASH_REPLACE_CHAR, '/');
    }
    return childGroups;
}
