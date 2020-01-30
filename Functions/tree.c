#include "../Headers/tree.h"
#include "../Headers/utility.h"

FileNode CreateFileNode(char* path) {
	FileNode temp = NULL; char* tempHash = NULL;
	temp = (FileNode)malloc(sizeof(*temp));

	if (temp == NULL)
		return NULL;

	strcpy(temp->fileName, (strrchr(path, '/') + 1));
	strcpy(temp->filePath, path);
	tempHash = hash(path);
	if (tempHash == NULL)
		return NULL;

	strcpy(temp->fileHash,tempHash);
	free(tempHash);
	temp->fileState = FILESTATE_COMMITTED;
	temp->foreignFlag = 0;
	temp->nextFile = NULL;

	return temp;
}

FolderNode CreateFolderNode(char* path) {
	FolderNode temp = NULL;
	temp = (FolderNode)malloc(sizeof(*temp));

	if (temp == NULL)
		return NULL;

	strcpy(temp->folderName, (strrchr(path, '/') + 1));
	strcpy(temp->folderPath, path);
	temp->fileList = NULL;
	temp->nextSibling = NULL;
	temp->firstChild = NULL;

	return temp;
}

int DeallocateFolderNode(FolderNode toDeallocate) { //posaljes jedan folder, moras dealocirati njegovo djete i na kraju njega nakon sto prije zvanja funkcije postavis firstchild->nextsibling
	FolderNode currentFolder = toDeallocate, temp = NULL;

	currentFolder = toDeallocate;
	while (currentFolder!= NULL) {
		if (currentFolder->firstChild != NULL)
			DeallocateFolderNode(currentFolder->firstChild);

		DeallocateFileList(currentFolder->fileList);

		temp = currentFolder;
		currentFolder = currentFolder->nextSibling;
		free(temp);
	}

	return RETURN_OK;
}

int DeallocateFileList(FileNode fileNode) {
	if (fileNode == NULL)
		return RETURN_OK;

	DeallocateFileList(fileNode->nextFile);
	free(fileNode);

	return RETURN_OK;
}

int InsertChild(FolderNode parentFolder, FolderNode toInsert) {
	FolderNode firstChild = parentFolder->firstChild; //da bi se moglo setati po listi dijece i siblinga bez utjecanja na originalnu strukturu

	if (parentFolder->firstChild == NULL)
		parentFolder->firstChild = toInsert;
	else {
		while (firstChild->nextSibling != NULL)
			firstChild = firstChild->nextSibling;

		firstChild->nextSibling = toInsert;
	}

	return RETURN_OK;
}

int AppendFile(FolderNode folder, FileNode fileToInsert) {
	FileNode firstFile = folder->fileList;

	if (folder->fileList == NULL) {
		folder->fileList = fileToInsert;
		return RETURN_OK;
	}
	else {
		while (firstFile->nextFile != NULL) {
			firstFile = firstFile->nextFile;
			if(!_strcmpi(firstFile->fileName, fileToInsert->fileName))
				return RETURN_OK;
		}
		firstFile->nextFile = fileToInsert;
	}

	return RETURN_OK;
}

FileNode FindFile(FolderNode fileTree, char* path) {
	FolderNode currentFolder = NULL; FileNode currentFile = NULL;
	FileNode result = NULL;
	currentFolder = fileTree;

	while (currentFolder != NULL) {
		if (currentFolder->firstChild != NULL)
			result=FindFile(currentFolder->firstChild, path);

		if (result != NULL)
			return result;

		currentFile = currentFolder->fileList;
		while (currentFile != NULL) {
			if (!_strcmpi(path, currentFile->filePath))
				return currentFile;
			currentFile = currentFile->nextFile;
		}

		currentFolder = currentFolder->nextSibling;
	}
	
	return result;
}

int CheckFilestate(FolderNode folderTree, int* stageNum) {
	FolderNode currentFolder = NULL; FileNode currentFile = NULL;
	currentFolder = folderTree;
	currentFile = currentFolder->fileList;

	if (currentFolder == NULL)
		return RETURN_OK;

	if (currentFolder->firstChild != NULL)
		CheckFilestate(currentFolder->firstChild, stageNum);

	while (currentFile != NULL) {
		if (currentFile->fileState == FILESTATE_STAGED) {
			printf("File staged but not committed: [%s]\n", currentFile->fileName);
			(*stageNum)++;
		}

		currentFile = currentFile->nextFile;
	}

	if (currentFolder->nextSibling != NULL)
		CheckFilestate(currentFolder->nextSibling, stageNum);


	return RETURN_OK;
}

int StageForCommit(FolderNode parentFolder, char* passedPath, int replacePosition, FolderNode parentCommitFileTree) //odvija se prije push Commit
{
	WIN32_FIND_DATA fdFile;
	HANDLE hFind = NULL;
	wchar_t sPath[BUFFER_SIZE];
	wchar_t sDir[BUFFER_SIZE];
	FolderNode tempFolder = NULL;
	FileNode tempFile = NULL;
	char pathBuffer[BUFFER_SIZE], foreignFilePath[BUFFER_SIZE];
	char* pathToFile = NULL, * foreignFileHash = NULL;
	int errnum = 0;

	mbstowcs(sDir, passedPath, BUFFER_SIZE);
	wsprintf(sPath, L"%s/*.*", sDir);

	if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE)
	{
		wprintf(L"Path not found: [%s]\n", sDir);
		return RETURN_NOEXIST;
	}

	do
	{
		if (wcscmp(fdFile.cFileName, L".") != 0 && wcscmp(fdFile.cFileName, L"..") != 0 && wcscmp(fdFile.cFileName, L".commit") != 0 && wcscmp(fdFile.cFileName, L".git") != 0)
		{
			wsprintf(sPath, L"%s/%s", sDir, fdFile.cFileName); //path do foldera/filea na disku
			wcstombs(pathBuffer, sPath, BUFFER_SIZE); // -||-

			if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				tempFolder = CreateFolderNode(pathBuffer);
				if (tempFolder == NULL) {
					return RETURN_ERROR_MEM_ALLOC;
				}

				InsertChild(parentFolder, tempFolder);
				errnum=StageForCommit(tempFolder, pathBuffer, replacePosition, parentCommitFileTree);
				if (errnum != RETURN_OK) {
					return errnum;
				}
			}
			else {
				pathToFile = pathBuffer + replacePosition + 1;
				snprintf(foreignFilePath, BUFFER_SIZE, "%s/%s", parentCommitFileTree->folderPath, pathToFile);
				tempFile = CreateFileNode(pathBuffer);
				if (tempFile == NULL)
					return RETURN_ERROR_MEM_ALLOC;

				foreignFileHash = hash(foreignFilePath);
				tempFile->fileState = FILESTATE_STAGED;

				if (foreignFileHash == NULL) {
					AppendFile(parentFolder, tempFile);
					free(foreignFileHash);
					continue;
				}

				if (!_strcmpi(tempFile->fileHash, foreignFileHash)) {
					tempFile->foreignFlag = 1;
					strcpy(tempFile->filePath, foreignFilePath);
				}

				AppendFile(parentFolder, tempFile);
				free(foreignFileHash);
			}
		}
	} while (FindNextFile(hFind, &fdFile));

	FindClose(hFind);
	return RETURN_OK;
}

int StageForBranch(FolderNode parentFolder) {
	FolderNode currentFolder = parentFolder;
	FileNode currentFile = currentFolder->fileList;

	if (currentFolder == NULL)
		return RETURN_OK;

	if (currentFolder->firstChild != NULL)
		StageForBranch(currentFolder->firstChild);

	while (currentFile != NULL) {
		currentFile->foreignFlag = 1;
		currentFile = currentFile->nextFile;
	}

	if (currentFolder->nextSibling != NULL)
		StageForBranch(currentFolder->nextSibling);


	return RETURN_OK;

}