using Xunit;

namespace SimulatorApp.Tests;

public class PlaceholderTest
{
    [Fact]
    public void Placeholder_AlwaysPasses()
    {
        // SimulatorApp is a WPF project (net8.0-windows) requiring Windows;
        // actual UI tests will be added when a Windows test runner is available.
        Assert.True(true);
    }
}
