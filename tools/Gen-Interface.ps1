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
Write-Output @"
class $ClassName;
NVRHI_CCLSID($ClassName, `"$GuidLiteral`")
class $ClassName final : public nvrhi::ObjectImpl<$BaseList>
{
public:
    NVRHI_DECLARE_UUID_TRAITS($ClassName)

    NVRHI_BEGIN_INTERFACE_TABLE_INLINE($ClassName)
    NVRHI_IMPLEMENTS_INTERFACE($ClassName)
    NVRHI_END_INTERFACE_TABLE_ROUTE_PARENT()
};
"@
}
