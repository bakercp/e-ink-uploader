//
// Copyright (c) 2013 Christopher Baker <https://christopherbaker.net>
//
// SPDX-License-Identifier:	MIT
//


#include "ofApp.h"
#include "Poco/PipeStream.h"
#include "Poco/Process.h"


void ofApp::setup()
{
    ofSetLogLevel(OF_LOG_VERBOSE);

    ofSetFrameRate(30);

    ofxHTTP::SimplePostServerSettings settings;

    // Many other settings are available.
    settings.setPort(7890);

    // If you want to set an alternate route path pattern (defaut is /post),
    // you can change the POST endpoint here. Just be sure to also update it in
    // any HTML forms in the bin/data/DocumentRoot.
    // settings.postRouteSettings.setRoutePathPattern("/");

    // Apply the settings.
    server.setup(settings);

    // The client can listen for POST form and multi-part upload events.
    // User be aware, these methods are called from other threads.
    // The user is responsible for protecting shared resources (e.g. ofMutex).
    server.postRoute().registerPostEvents(this);

    // Start the server.
    server.start();

#if !defined(TARGET_LINUX_ARM)
    // Launch a browser with the address of the server.
    ofLaunchBrowser(server.url());
#endif

    einkPath = "/bin/pwd";

}


void ofApp::update()
{
    std::filesystem::path path;
    if (imageQueue.tryReceive(path))
    {
        std::vector<std::string> args;

//        args.push_back("hashtag");
//        args.push_back(_hashtag);
//        args.push_back(_downloadPath.string());
//        if (_quiet)
//        {
//            args.push_back("--quiet");
//        }
//        args.push_back("--new");
//        args.push_back("-n " + std::to_string(_numImagesToDownload));
//        args.push_back("-T" + FILENAME_TEMPLATE);
//        if (!_username.empty() || !_password.empty())
//        {
//            args.push_back("-c" + _username + ":" + _password);
//        }

        Poco::Pipe outPipe;

        Poco::ProcessHandle handle = Poco::Process::launch(einkPath,
                                                           args,
                                                           nullptr,
                                                           &outPipe,
                                                           &outPipe);
        Poco::PipeInputStream istr(outPipe);

        std::atomic<int> exitCode(0);

        std::thread processThread([&](){
            std::string outString;
            Poco::StreamCopier::copyToString(istr, outString);
            ofLogVerbose("HashtagClient::_loot") << "Process Output: " << outString;

            try
            {
                exitCode = handle.wait();
            }
            catch (const std::exception& exc)
            {
                ofLogWarning("HashtagClient::_loot") << "Process Thread: " << exc.what();
            }
        });

        bool didKill = false;

        uint64_t startTime = ofGetElapsedTimeMillis();

        while (true)
        {
            if (Poco::Process::isRunning(handle))
            {
                uint64_t now = ofGetElapsedTimeMillis();

                if (now < (startTime + DEFAULT_PROCESS_TIMEOUT))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(PROCESS_THREAD_SLEEP));
                }
                else break;
            }
            else break;
        }

        if (Poco::Process::isRunning(handle))
        {
            ofLogWarning("HashtagClient::_loot") << "Process timed out, killing.";
            Poco::Process::kill(handle);
            didKill = true;
        }

        try
        {
            exitCode = handle.wait();
        }
        catch (const std::exception& exc)
        {
        }
        
        try
        {
            ofLogVerbose("HashtagClient::_loot") << "Joining Process ...";
            processThread.join();
        }
        catch (const std::exception& exc)
        {
            ofLogVerbose("HashtagClient::_loot") << "Exit: " << exc.what();
        }
        
        ofLogVerbose("HashtagClient::_loot") << "... joined and process exited with code: " << exitCode;
    }
}


void ofApp::draw()
{
    ofBackground(255);
    ofDrawBitmapStringHighlight("See " + server.url(), 10, 16);
    ofDrawBitmapStringHighlight("See the Console", 10, 42);
}


void ofApp::onHTTPPostEvent(ofxHTTP::PostEventArgs& args)
{
    ofLogNotice("ofApp::onHTTPPostEvent") << "Data: " << args.getBuffer().getText();
}


void ofApp::onHTTPFormEvent(ofxHTTP::PostFormEventArgs& args)
{
    ofLogNotice("ofApp::onHTTPFormEvent") << "";
    ofxHTTP::HTTPUtils::dumpNameValueCollection(args.getForm(), ofGetLogLevel());
}


void ofApp::onHTTPUploadEvent(ofxHTTP::PostUploadEventArgs& args)
{
    std::string stateString = "";

    switch (args.getState())
    {
        case ofxHTTP::PostUploadEventArgs::UPLOAD_STARTING:
            stateString = "STARTING";
            break;
        case ofxHTTP::PostUploadEventArgs::UPLOAD_PROGRESS:
            stateString = "PROGRESS";
            break;
        case ofxHTTP::PostUploadEventArgs::UPLOAD_FINISHED:
            imageQueue.send(args.getFilename());
            stateString = "FINISHED";
            break;
    }

    ofLogNotice("ofApp::onHTTPUploadEvent") << "";
    ofLogNotice("ofApp::onHTTPUploadEvent") << "         state: " << stateString;
    ofLogNotice("ofApp::onHTTPUploadEvent") << " formFieldName: " << args.getFormFieldName();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "orig. filename: " << args.getOriginalFilename();
    ofLogNotice("ofApp::onHTTPUploadEvent") <<  "     filename: " << args.getFilename();
    ofLogNotice("ofApp::onHTTPUploadEvent") <<  "     fileType: " << args.getFileType().toString();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "# bytes xfer'd: " << args.getNumBytesTransferred();
}
