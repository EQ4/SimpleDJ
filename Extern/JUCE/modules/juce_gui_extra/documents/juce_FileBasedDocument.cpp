/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-11 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

FileBasedDocument::FileBasedDocument (const String& fileExtension_,
                                      const String& fileWildcard_,
                                      const String& openFileDialogTitle_,
                                      const String& saveFileDialogTitle_)
    : changedSinceSave (false),
      fileExtension (fileExtension_),
      fileWildcard (fileWildcard_),
      openFileDialogTitle (openFileDialogTitle_),
      saveFileDialogTitle (saveFileDialogTitle_)
{
}

FileBasedDocument::~FileBasedDocument()
{
}

//==============================================================================
void FileBasedDocument::setChangedFlag (const bool hasChanged)
{
    if (changedSinceSave != hasChanged)
    {
        changedSinceSave = hasChanged;
        sendChangeMessage();
    }
}

void FileBasedDocument::changed()
{
    changedSinceSave = true;
    sendChangeMessage();
}

//==============================================================================
void FileBasedDocument::setFile (const File& newFile)
{
    if (documentFile != newFile)
    {
        documentFile = newFile;
        changed();
    }
}

//==============================================================================
#if JUCE_MODAL_LOOPS_PERMITTED
bool FileBasedDocument::loadFrom (const File& newFile,
                                  const bool showMessageOnFailure)
{
    MouseCursor::showWaitCursor();

    const File oldFile (documentFile);
    documentFile = newFile;

    Result result (Result::fail ("The file doesn't exist"));

    if (newFile.existsAsFile())
    {
        result = loadDocument (newFile);

        if (result.wasOk())
        {
            setChangedFlag (false);
            MouseCursor::hideWaitCursor();

            setLastDocumentOpened (newFile);
            return true;
        }
    }

    documentFile = oldFile;
    MouseCursor::hideWaitCursor();

    if (showMessageOnFailure)
    {
        AlertWindow::showMessageBox (AlertWindow::WarningIcon,
                                     TRANS("Failed to open file..."),
                                     TRANS("There was an error while trying to load the file:\n\n")
                                       + newFile.getFullPathName()
                                       + "\n\n"
                                       + result.getErrorMessage());
    }

    return false;
}

bool FileBasedDocument::loadFromUserSpecifiedFile (const bool showMessageOnFailure)
{
    FileChooser fc (openFileDialogTitle,
                    getLastDocumentOpened(),
                    fileWildcard);

    if (fc.browseForFileToOpen())
        return loadFrom (fc.getResult(), showMessageOnFailure);

    return false;
}

//==============================================================================
FileBasedDocument::SaveResult FileBasedDocument::save (const bool askUserForFileIfNotSpecified,
                                                       const bool showMessageOnFailure)
{
    return saveAs (documentFile,
                   false,
                   askUserForFileIfNotSpecified,
                   showMessageOnFailure);
}

FileBasedDocument::SaveResult FileBasedDocument::saveAs (const File& newFile,
                                                         const bool warnAboutOverwritingExistingFiles,
                                                         const bool askUserForFileIfNotSpecified,
                                                         const bool showMessageOnFailure)
{
    if (newFile == File::nonexistent)
    {
        if (askUserForFileIfNotSpecified)
        {
            return saveAsInteractive (true);
        }
        else
        {
            // can't save to an unspecified file
            jassertfalse;
            return failedToWriteToFile;
        }
    }

    if (warnAboutOverwritingExistingFiles && newFile.exists())
    {
        if (! AlertWindow::showOkCancelBox (AlertWindow::WarningIcon,
                                            TRANS("File already exists"),
                                            TRANS("There's already a file called:\n\n")
                                              + newFile.getFullPathName()
                                              + TRANS("\n\nAre you sure you want to overwrite it?"),
                                            TRANS("overwrite"),
                                            TRANS("cancel")))
        {
            return userCancelledSave;
        }
    }

    MouseCursor::showWaitCursor();

    const File oldFile (documentFile);
    documentFile = newFile;

    const Result result (saveDocument (newFile));

    if (result.wasOk())
    {
        setChangedFlag (false);
        MouseCursor::hideWaitCursor();

        return savedOk;
    }

    documentFile = oldFile;
    MouseCursor::hideWaitCursor();

    if (showMessageOnFailure)
    {
        AlertWindow::showMessageBox (AlertWindow::WarningIcon,
                                     TRANS("Error writing to file..."),
                                     TRANS("An error occurred while trying to save \"")
                                        + getDocumentTitle()
                                        + TRANS("\" to the file:\n\n")
                                        + newFile.getFullPathName()
                                        + "\n\n"
                                        + result.getErrorMessage());
    }

    return failedToWriteToFile;
}

FileBasedDocument::SaveResult FileBasedDocument::saveIfNeededAndUserAgrees()
{
    if (! hasChangedSinceSaved())
        return savedOk;

    const int r = AlertWindow::showYesNoCancelBox (AlertWindow::QuestionIcon,
                                                   TRANS("Closing document..."),
                                                   TRANS("Do you want to save the changes to \"")
                                                       + getDocumentTitle() + "\"?",
                                                   TRANS("save"),
                                                   TRANS("discard changes"),
                                                   TRANS("cancel"));

    if (r == 1)
    {
        // save changes
        return save (true, true);
    }
    else if (r == 2)
    {
        // discard changes
        return savedOk;
    }

    return userCancelledSave;
}

File FileBasedDocument::getSuggestedSaveAsFile (const File& defaultFile)
{
    return defaultFile.withFileExtension (fileExtension).getNonexistentSibling (true);
}

FileBasedDocument::SaveResult FileBasedDocument::saveAsInteractive (const bool warnAboutOverwritingExistingFiles)
{
    File f;

    if (documentFile.existsAsFile())
        f = documentFile;
    else
        f = getLastDocumentOpened();

    String legalFilename (File::createLegalFileName (getDocumentTitle()));

    if (legalFilename.isEmpty())
        legalFilename = "unnamed";

    if (f.existsAsFile() || f.getParentDirectory().isDirectory())
        f = f.getSiblingFile (legalFilename);
    else
        f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (legalFilename);

    f = getSuggestedSaveAsFile (f);

    FileChooser fc (saveFileDialogTitle, f, fileWildcard);

    if (fc.browseForFileToSave (warnAboutOverwritingExistingFiles))
    {
        File chosen (fc.getResult());
        if (chosen.getFileExtension().isEmpty())
        {
            chosen = chosen.withFileExtension (fileExtension);

            if (chosen.exists())
            {
                if (! AlertWindow::showOkCancelBox (AlertWindow::WarningIcon,
                                                    TRANS("File already exists"),
                                                    TRANS("There's already a file called:")
                                                      + "\n\n" + chosen.getFullPathName()
                                                      + "\n\n" + TRANS("Are you sure you want to overwrite it?"),
                                                    TRANS("overwrite"),
                                                    TRANS("cancel")))
                {
                    return userCancelledSave;
                }
            }
        }

        setLastDocumentOpened (chosen);
        return saveAs (chosen, false, false, true);
    }

    return userCancelledSave;
}
#endif
