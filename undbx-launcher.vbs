'
'   UnDBX - Tool to extract e-mail messages from Outlook Express DBX files.
'   Copyright (C) 2008, 2009 Avi Rozen <avi.rozen@gmail.com>
'
'   DBX file format parsing code is based on
'   DbxConv - a DBX to MBOX Converter.
'   Copyright (C) 2008 Ulrich Krebs <ukrebs@freenet.de>
'
'   This file is part of UnDBX.
'
'   UnDBX is free software: you can redistribute it and/or modify
'   it under the terms of the GNU General Public License as published by
'   the Free Software Foundation, either version 3 of the License, or
'   (at your option) any later version.
'
'   This program is distributed in the hope that it will be useful,
'   but WITHOUT ANY WARRANTY; without even the implied warranty of
'   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
'   GNU General Public License for more details.
'
'   You should have received a copy of the GNU General Public License
'   along with this program.  If not, see <http://www.gnu.org/licenses/>.
'

Option Explicit

Dim WshShell
Dim objFSO
Dim strUndbxExe

Set WshShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")

' This script is meant to be installed in the same folder as undbx.exe
strUndbxExe = objFSO.GetAbsolutePathName(objFSO.BuildPath(Wscript.ScriptFullName, ".."))&"\undbx.exe"
If Not objFSO.FileExists(strUndbxExe) Then
    Wscript.Echo "Error: UnDBX not found - " & strUndbxExe
Else
    RunUndbx strUndbxExe
End If

Set objFSO =  Nothing
Set WshShell = Nothing



Sub RunUndbx(strUndbxExe)

    '
    ' Ask the user to select input and output folders, and then launch
    ' the extraction process.
    '

    Dim strInFolder, strOutFolder
    Dim objApp

    If isProcessRunning("msimn.exe") Then
        Wscript.Echo "Please exit Outlook Express before launching UnDBX"
        Exit Sub
    End If

    strInFolder = GetFolder("Please select .dbx INPUT folder:")
    If strInFolder = "" Then
        Exit Sub
    End If 
    
    strOutFolder = GetFolder("Please select .eml OUTPUT folder:")
    If strOutFolder = "" Then
        Exit Sub
    End If 

    Set objApp = CreateObject("WScript.Shell")
    ' quoting is a bitch: the following should work even when all file paths contain spaces
    objApp.Run "cmd /c """"" & strUndbxExe & """ """ & strInFolder & """ """ & strOutFolder & """ & pause""" 
    Set objApp = Nothing

End Sub 


Function isProcessRunning(strExe)

    '
    ' Check if the specified executable is running
    '
    
    Dim strComputer
    Dim objWMIService
    Dim colProcesses
    
    strComputer = "."
    Set objWMIService = GetObject("winmgmts:" _
                                  & "{impersonationLevel=impersonate}!\\" & strComputer & "\root\cimv2")

    Set colProcesses = objWMIService.ExecQuery _
        ("Select * from Win32_Process Where Name = '"&strExe&"'")

    isProcessRunning = colProcesses.Count > 0

    Set colProcesses = Nothing
    Set objWMIService = Nothing

End Function


Function GetFolder(strPrompt)

    ' 
    ' Ask user to select a folder. Based on the following article:
    ' http://www.microsoft.com/technet/scriptcenter/resources/qanda/jun05/hey0617.mspx
    '

    Const MY_COMPUTER = &H11&
    Const WINDOW_HANDLE = 0
    Const OPTIONS = &H10&

    Dim objShell
    Dim objFolder
    Dim objFolderItem
    Dim strPath
    
    Set objShell = CreateObject("Shell.Application")
    Set objFolder = objShell.Namespace(MY_COMPUTER)
    Set objFolderItem = objFolder.Self
    strPath = objFolderItem.Path
    
    Set objFolder = objShell.BrowseForFolder _
        (WINDOW_HANDLE, strPrompt, OPTIONS, strPath)
    
    If objFolder Is Nothing Then
        GetFolder = ""
        Exit Function
    End If
    
    Set objFolderItem = objFolder.Self
    strPath = objFolderItem.Path
    
    GetFolder = strPath

    Set objFolder = Nothing
    Set objShell = Nothing
    
End Function
