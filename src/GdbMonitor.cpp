// SPDX-FileCopyrightText: 2021 Ernie Pasveer <epasveer@att.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "GdbMonitor.h"
#include <QtCore/QtCore>
#include <QtCore/QProcess>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDebug>
#include <iostream>

static QLoggingCategory LC("seer.gdbmonitor");

GdbMonitor::GdbMonitor (QObject* parent) : QObject(parent) {
    _process = 0;
}

GdbMonitor::~GdbMonitor () {
}

void GdbMonitor::handleErrorOccurred (QProcess::ProcessError error) {

    Q_UNUSED(error);

    qCDebug(LC) << error;

    qApp->exit();
}

void GdbMonitor::handleFinished (int exitCode, QProcess::ExitStatus exitStatus) {

    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    qCDebug(LC) << exitCode << exitStatus;

    qApp->exit();
}

void GdbMonitor::handleReadyReadStandardError () {

    qCDebug(LC) << "Ready to read stderr";

    QProcess* p = (QProcess*)sender();

    // canReadLine()/readLine() work on the current read channel, which
    // defaults to stdout. Point them at stderr for this handler, then
    // switch back for the stdout handler.
    p->setReadChannel(QProcess::StandardError);

    // Read a line at a time.
    while (p->canReadLine()) {

        QByteArray buf = p->readLine();

        // Chop off trailing RETURN character.
        if (buf[buf.size()-1] == '\n') {
            buf.chop(1);
        }

        // Convert to a string.
        QString text(buf);

        qCDebug(LC) << text;

        // Start broadcasting it around.
        emit allTextOutput(text);
    }

    p->setReadChannel(QProcess::StandardOutput);
}

void GdbMonitor::handleReadyReadStandardOutput () {

    qCDebug(LC) << "Ready to read stdout";

    QProcess* p = (QProcess*)sender();

    // Read a line at a time.
    while (p->canReadLine()) {

        QByteArray buf = p->readLine();

        // Chop off trailing RETURN character.
        if (buf[buf.size()-1] == '\n') {
            buf.chop(1);
        }

        if (buf.size() == 0) { // Ignore empty lines.
            continue;
        }

        // Convert to a string.
        QString text(buf);

#if defined(Q_OS_WIN)
        // gdb 17.x reports 'fullname' fields MI-escaped with backslashes
        // while its symbol table uses forward slashes. Rewrite the value
        // of every 'fullname="..."' field to forward slashes here, once,
        // so all consumers (editor keys, breakpoint dialogs, tables)
        // see a single spelling. Case is preserved. The MI escape pair
        // '\\' becomes one '/', so the line is rebuilt by segments.
        {
            static const QRegularExpression fullnameRe("fullname=\"[^\"]*\"");
            QRegularExpressionMatchIterator it = fullnameRe.globalMatch(text);

            QString newText;
            int     last = 0;

            while (it.hasNext()) {
                QRegularExpressionMatch m = it.next();
                newText += text.mid(last, m.capturedStart() - last);

                QString value = m.captured(0);
                value.replace("\\\\", "/");
                newText += value;

                last = m.capturedStart() + m.capturedLength();
            }

            newText += text.mid(last);
            text = newText;
        }
#endif

        //qDebug() << "Read buffer" << buf.size() << (int)buf[buf.size()-1] << text;
        qCDebug(LC) << text;

#if SEER_GDB_LOGOUT == 1
        // Start broadcasting it around. For debugging
        emit allTextOutput("From GdbMonitor: " + text + "\n");
#endif

        if (text[0] == '~') {
            emit tildeTextOutput(text);
        }else if (text[0] == '=') {
            emit equalTextOutput(text);
        }else if (text[0] == '*') {
            emit astrixTextOutput(text);
        }else if (text[0] == '^') {
            emit caretTextOutput(text);
        }else if (text[0] == '&') {
            emit ampersandTextOutput(text);
        }else if (text[0] == '@') {
            emit atsignTextOutput(text);
        }else if (text.contains(QRegularExpression("^([0-9]+)\\~"))) {
            emit tildeTextOutput(text);
        }else if (text.contains(QRegularExpression("^([0-9]+)\\="))) {
            emit equalTextOutput(text);
        }else if (text.contains(QRegularExpression("^([0-9]+)\\*"))) {
            emit astrixTextOutput(text);
        }else if (text.contains(QRegularExpression("^([0-9]+)\\^"))) {
            emit caretTextOutput(text);
        }else if (text.contains(QRegularExpression("^([0-9]+)\\&"))) {
            emit ampersandTextOutput(text);
        }else{
            emit textOutput(text);
        }
    }

    qCDebug(LC) << "Finished reading stdout";
}

void GdbMonitor::handleTextOutput (QString text) {

    qCDebug(LC) << "Ready to handle text output";
    qCDebug(LC) << text;

    emit allTextOutput(text);

    if (text[0] == '~') {
        emit tildeTextOutput(text);
    }else if (text[0] == '=') {
        emit equalTextOutput(text);
    }else if (text[0] == '*') {
        emit astrixTextOutput(text);
    }else if (text[0] == '^') {
        emit caretTextOutput(text);
    }else if (text[0] == '&') {
        emit ampersandTextOutput(text);
    }else if (text.contains(QRegularExpression("^([0-9]+)\\~"))) {
        emit tildeTextOutput(text);
    }else if (text.contains(QRegularExpression("^([0-9]+)\\="))) {
        emit equalTextOutput(text);
    }else if (text.contains(QRegularExpression("^([0-9]+)\\*"))) {
        emit astrixTextOutput(text);
    }else if (text.contains(QRegularExpression("^([0-9]+)\\^"))) {
        emit caretTextOutput(text);
    }else if (text.contains(QRegularExpression("^([0-9]+)\\&"))) {
        emit ampersandTextOutput(text);
    }else{
        emit textOutput(text);
    }

    qCDebug(LC) << "Finished handling text output";
}

void GdbMonitor::handleStarted() {

    qCDebug(LC);
}

void GdbMonitor::handleStateChanged(QProcess::ProcessState newState) {

    Q_UNUSED(newState);

    qCDebug(LC) << newState;
}

void GdbMonitor::setProcess (QProcess* process) {
    _process = process;
}

QProcess* GdbMonitor::process () {
    return _process;
}

