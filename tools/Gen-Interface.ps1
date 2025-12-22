[CmdletBinding(DefaultParameterSetName = 'Interface')]
param(
    [Parameter(HelpMessage = "Interface/Class Name", ParameterSetName='Interface')]
    [string]$InterfaceName,
    [Parameter(HelpMessage = "Class Name", ParameterSetName='Class')]
    [string]$ClassName
)

$TypeName = ''
if ($InterfaceName) {
    $TypeName = $InterfaceName
} else {
    $TypeName = $ClassName
}

$CurGuid = New-Guid


$GuidLiteral = $CurGuid.ToString('D')

if($InterfaceName) {
Write-Output @"
DONUT_IID($TypeName, `"$GuidLiteral`")
struct $TypeName : IObject {
DONUT_DECLARE_UUID_TRAITS($TypeName)
};
"@

} else {
    Write-Output @"
DONUT_CLSID($TypeName, `"$GuidLiteral`")
struct $TypeName {
    DONUT_DECLARE_UUID_TRAITS($TypeName)
    DONUT_DECLARE_INTERFACE_TABLE()
};
"@
}