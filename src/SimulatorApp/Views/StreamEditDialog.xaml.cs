using System.Windows;

namespace SimulatorApp.Views
{
    public partial class StreamEditDialog : Window
    {
        public string DstMac  { get; private set; } = "";
        public string SrcMac  { get; private set; } = "";
        public string DstIp   { get; private set; } = "";
        public string SrcIp   { get; private set; } = "";
        public string DstPort { get; private set; } = "";
        public string SrcPort { get; private set; } = "";

        public StreamEditDialog(
            string dstMac = "", string srcMac = "",
            string dstIp  = "", string srcIp  = "",
            string dstPort = "", string srcPort = "")
        {
            InitializeComponent();
            TxtDstMac.Text  = dstMac;
            TxtSrcMac.Text  = srcMac;
            TxtDstIp.Text   = dstIp;
            TxtSrcIp.Text   = srcIp;
            TxtDstPort.Text = dstPort;
            TxtSrcPort.Text = srcPort;
        }

        private void OkButton_Click(object sender, RoutedEventArgs e)
        {
            DstMac  = TxtDstMac.Text.Trim();
            SrcMac  = TxtSrcMac.Text.Trim();
            DstIp   = TxtDstIp.Text.Trim();
            SrcIp   = TxtSrcIp.Text.Trim();
            DstPort = TxtDstPort.Text.Trim();
            SrcPort = TxtSrcPort.Text.Trim();
            DialogResult = true;
        }

        private void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
        }
    }
}
