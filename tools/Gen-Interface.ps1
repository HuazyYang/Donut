<#
.SYNOPSIS
Prints the declaration skeleton of an nvrhi::core interface or implementation class with a fresh GUID.

.EXAMPLE
.\Gen-Interface.ps1 -InterfaceName IFoo                       # struct IFoo : nvrhi::IObject
.\Gen-Interface.ps1 -InterfaceName IBar -Parent nvrhi::IRHIObject
.\Gen-Interface.ps1 -ClassName Foo -Bases IFoo                 # class Foo final : public nvrhi::ObjectImpl<IFoo>
#>
[CmdletBinding(DefaultParameterSetName = 'Interface')]
param(
    [Parameter(Mandatory, HelpMessage = "Interface Name", ParameterSetName = 'Interface')]
    [string]$InterfaceName,
    [Parameter(HelpMessage = "Parent interface", ParameterSetName = 'Interface')]
    [string]$Parent = 'nvrhi::IObject',
    [Parameter(Mandatory, HelpMessage = "Class Name", ParameterSetName = 'Class')]
    [string]$ClassName,
    [Parameter(HelpMessage = "ObjectImpl bases (interfaces the class implements)", ParameterSetName = 'Class')]
    [string[]]$Bases = @('nvrhi::IObject')
)

# The GUID parser only accepts lowercase hex digits.
$GuidLiteral = (New-Guid).ToString('D').ToLowerInvariant()

if ($InterfaceName) {
Write-Output @"
NVRHI_IID($InterfaceName, `"$GuidLiteral`")
struct $InterfaceName : $Parent
{
    NVRHI_DECLARE_UUID_TRAITS_DERIVED($InterfaceName, $Parent)
};
"@
} else {
$BaseList = $Bases -join ', '
# NVRHI_CLASS_CLSID forward-declares the class and gives it the class ID; NVRHI_CLASS_INTERFACE_TABLE answers
# it (what checked_cast and the QueryInterface type tests use, nvrhi ADR 0006) and routes the rest to ObjectImpl.
Write-Output @"
NVRHI_CLASS_CLSID($ClassName, `"$GuidLiteral`")
class $ClassName final : public nvrhi::ObjectImpl<$BaseList>
{
public:
    NVRHI_CLASS_INTERFACE_TABLE($ClassName)
};
"@
}
