/// FicTrac http://rjdmoore.net/fictrac/
/// \file       Recorder.cpp
/// \brief      Simple threaded writer.
/// \author     Richard Moore
/// \copyright  CC BY-NC-SA 3.0

#include "Recorder.h"

#include "TermRecorder.h"
#include "FileRecorder.h"
#include "SocketRecorder.h"
#include "SerialRecorder.h"
#include "misc.h"   // thread priority

#include <iostream> // cout/cerr

using namespace std;

Recorder::Recorder(RecorderInterface::RecordType type, string fn)
    : _active(false), _accepting_messages(false), _write_failed(false)
{
    /// Set record type.
    switch (type) {
    case RecorderInterface::RecordType::TERM:
        _record = make_unique<TermRecorder>();
        break;
    case RecorderInterface::RecordType::FILE:
        _record = make_unique<FileRecorder>();
        break;
    case RecorderInterface::RecordType::SOCK:
        _record = make_unique<SocketRecorder>();
        break;
    case RecorderInterface::RecordType::COM:
        _record = make_unique<SerialRecorder>();
        break;
    default:
        break;
    }

    /// Open record and start async recording.
    if (_record && _record->openRecord(fn)) {
        _active = true;
        _accepting_messages = true;
        _thread = make_unique<thread>(&Recorder::processMsgQ, this);
    }
    else {
        cerr << "Error initialising recorder!" << endl;
    }
}

Recorder::~Recorder()
{
    cout << "Closing recorder.." << endl;

    unique_lock<mutex> l(_qMutex);
    _accepting_messages = false;
    _qCond.notify_all();
    l.unlock();

    if (_thread && _thread->joinable()) {
        _thread->join();
    }

    _active = false;

    if (_write_failed) {
        cerr << "Error! Recorder closed after one or more write failures." << endl;
    }

    /// _record->close() called by unique_ptr dstr.
}

bool Recorder::addMsg(string msg)
{
    bool ret = false;
    lock_guard<mutex> l(_qMutex);
    if (_accepting_messages) {
        _msgQ.push_back(msg);
        _qCond.notify_all();
        ret = true;
    }
    return ret;
}

void Recorder::processMsgQ()
{
    /// Set thread high priority (when run as SU).
    if (!SetThreadNormalPriority()) {
        cerr << "Error! Recorder processing thread unable to set thread priority!" << endl;
    }

    /// Get a un/lockable lock.
    unique_lock<mutex> l(_qMutex);
    while (_accepting_messages || !_msgQ.empty()) {
        while (_msgQ.size() == 0) {
            if (!_accepting_messages) {
                _active = false;
                return;
            }
            _qCond.wait(l);
        }

        /// Process queued messages before allowing the worker to exit.
        while (_msgQ.size() > 0) {
            string msg = _msgQ.front();
            _msgQ.pop_front();
            l.unlock();

            // do async i/o
            try {
                if (!_record->writeRecord(msg)) {
                    _write_failed = true;
                }
            }
            catch (...) {
                _write_failed = true;
            }
            l.lock();
        }
    }

    _active = false;
}
