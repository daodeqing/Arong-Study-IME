function IsPathInside(const Child, Parent: String): Boolean;
begin
  Result :=
    (CompareText(Child, Parent) = 0) or
    (CompareText(
       Copy(AddBackslash(Child), 1, Length(AddBackslash(Parent))),
       AddBackslash(Parent)) = 0);
end;

function InstallationPathsOverlap(const AppDir, DataDir: String): Boolean;
begin
  Result := IsPathInside(AppDir, DataDir) or IsPathInside(DataDir, AppDir);
end;
