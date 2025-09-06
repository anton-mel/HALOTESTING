#!/bin/bash

# Add MOC rules for our new pipeline classes

# Add to the MOC header list
sed -i '' 's/moc_spectrogramplot.cpp moc_viewfilterswindow.cpp/moc_spectrogramplot.cpp moc_viewfilterswindow.cpp \\\n            moc_pipelinedatarhxcontroller.cpp moc_pipelinedatamanager.cpp/' Makefile

# Add to the compiler_moc_header_make_all section
sed -i '' 's/moc_spectrogramplot.cpp$/moc_spectrogramplot.cpp moc_pipelinedatarhxcontroller.cpp moc_pipelinedatamanager.cpp/' Makefile

# Add to the clean section
sed -i '' 's/moc_viewfilterswindow.cpp$/moc_viewfilterswindow.cpp moc_pipelinedatarhxcontroller.cpp moc_pipelinedatamanager.cpp/' Makefile

# Add the actual MOC generation rules
cat >> Makefile << 'EOF'

moc_pipelinedatarhxcontroller.cpp: Engine/API/Synthetic/pipelinedatarhxcontroller.h \
	Engine/API/Abstract/abstractrhxcontroller.h \
	Engine/API/Hardware/rhxdatablock.h \
	Engine/API/Hardware/rhxglobals.h \
	/opt/homebrew/lib/QtCore.framework/Headers/QObject \
	/opt/homebrew/lib/QtCore/framework/Headers/qobject.h \
	/opt/homebrew/lib/QtCore/framework/Headers/QTimer \
	/opt/homebrew/lib/QtCore/framework/Headers/qtimer.h \
	/opt/homebrew/lib/QtCore/framework/Headers/QDebug \
	/opt/homebrew/lib/QtCore/framework/Headers/qdebug.h
	/opt/homebrew/share/qt/libexec/moc $(DEFINES) --include /Users/antonmelnychuk/Desktop/Intan-RHX/moc_predefs.h -I/opt/homebrew/share/qt/mkspecs/macx-clang -I/Users/antonmelnychuk/Desktop/Intan-RHX -I/Users/antonmelnychuk/Desktop/Intan-RHX -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Processing -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Processing/DataFileReaders -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Processing/SaveManagers -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Processing/XPUInterfaces -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Threads -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/API/Synthetic -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/API/Abstract -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/API/Hardware -I/Users/antonmelnychuk/Desktop/Intan-RHX/GUI/Dialogs -I/Users/antonmelnychuk/Desktop/Intan-RHX/GUI/Widgets -I/Users/antonmelnychuk/Desktop/Intan-RHX/GUI/Windows -I/Users/antonmelnychuk/Desktop/Intan-RHX/includes -I/opt/homebrew/lib/QtMultimedia.framework/Headers -I/opt/homebrew/lib/QtWidgets.framework/Headers -I/opt/homebrew/lib/QtGui.framework/Headers -I/opt/homebrew/lib/QtXml.framework/Headers -I/opt/homebrew/lib/QtNetwork.framework/Headers -I/opt/homebrew/lib/QtCore.framework/Headers -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1 -I/Library/Developer/CommandLineTools/usr/lib/clang/16/include -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include -I/Library/Developer/CommandLineTools/usr/include -F/opt/homebrew/lib Engine/API/Synthetic/pipelinedatarhxcontroller.h -o moc_pipelinedatarhxcontroller.cpp

moc_pipelinedatamanager.cpp: Engine/API/Synthetic/pipelinedatamanager.h \
	Engine/API/Synthetic/pipelinedatarhxcontroller.h \
	/opt/homebrew/lib/QtCore.framework/Headers/QObject \
	/opt/homebrew/lib/QtCore/framework/Headers/qobject.h \
	/opt/homebrew/lib/QtCore/framework/Headers/QTimer \
	/opt/homebrew/lib/QtCore/framework/Headers/qtimer.h \
	/opt/homebrew/lib/QtCore/framework/Headers/QDebug \
	/opt/homebrew/lib/QtCore/framework/Headers/qdebug.h
	/opt/homebrew/share/qt/libexec/moc $(DEFINES) --include /Users/antonmelnychuk/Desktop/Intan-RHX/moc_predefs.h -I/opt/homebrew/share/qt/mkspecs/macx-clang -I/Users/antonmelnychuk/Desktop/Intan-RHX -I/Users/antonmelnychuk/Desktop/Intan-RHX -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Processing -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Processing/DataFileReaders -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Processing/SaveManagers -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Processing/XPUInterfaces -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Threads -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/API/Synthetic -I/Users/antonmelnychuk/Desktop/Intan-RHX/Engine/Developer/CommandLineTools/usr/include -F/opt/homebrew/lib Engine/API/Synthetic/pipelinedatamanager.h -o moc_pipelinedatamanager.cpp

moc_pipelinedatarhxcontroller.o: moc_pipelinedatarhxcontroller.cpp 
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o moc_pipelinedatarhxcontroller.o moc_pipelinedatarhxcontroller.cpp

moc_pipelinedatamanager.o: moc_pipelinedatamanager.cpp 
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o moc_pipelinedatamanager.o moc_pipelinedatamanager.cpp
EOF

echo "MOC rules added to Makefile"
