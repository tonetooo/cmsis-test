#!/usr/bin/env python3
"""Convert HERMES-A1 HTML documentation to PDF"""
import sys
import os

try:
    from weasyprint import HTML
    from weasyprint.text_font_features import FontFeatures
    import logging
    
    # Configure logging
    logging.basicConfig(level=logging.WARNING)
    
    def html_to_pdf(html_path, pdf_path):
        """Convert HTML to PDF using WeasyPrint"""
        print(f"Converting {html_path} to {pdf_path}...")
        
        # Read HTML content
        with open(html_path, 'r', encoding='utf-8') as f:
            html_content = f.read()
        
        # Create HTML object
        html = HTML(string=html_content, base_url=os.path.dirname(html_path))
        
        # PDF options
        pdf_options = {
            'page_size': 'A4',
            'margin_top': 20,
            'margin_right': 20,
            'margin_bottom': 20,
            'margin_left': 20,
            'page_height': '297mm',
            'page_width': '210mm',
            'encoding': 'UTF-8',
            'vary_media': True,
            'optimize_size': True,
            'pdf_preset': 'a4',
        }
        
        # Create PDF
        html.write_pdf(pdf_path, **pdf_options)
        
        # Get file size
        file_size = os.path.getsize(pdf_path)
        print(f"PDF generated successfully: {pdf_path} ({file_size} bytes)")
        
        return True
    
    def main():
        # Paths
        html_path = "hermes_a1_dossier_v2.0.html"
        pdf_path = "hermes_a1_dossier_v2.0.pdf"
        
        # Check if HTML file exists
        if not os.path.exists(html_path):
            print(f"Error: HTML file not found: {html_path}")
            print("Available files:")
            for file in os.listdir('.'):
                if file.endswith('.html'):
                    print(f"  - {file}")
            return 1
        
        # Generate PDF
        try:
            html_to_pdf(html_path, pdf_path)
            print("\n✅ PDF generation completed successfully!")
            print(f"📄 File saved as: {pdf_path}")
            return 0
        except Exception as e:
            print(f"❌ Error generating PDF: {e}")
            return 1
    
    if __name__ == "__main__":
        sys.exit(main())

except ImportError:
    print("WeasyPrint is not installed.")
    print("\nTo install WeasyPrint, run:")
    print("pip install weasyprint")
    print("\nYou may also need system dependencies:")
    print("  - On Ubuntu/Debian: sudo apt-get install python3-dev python3-pip libpango-1.0-0 libharfbuzz0b-dev libcairo2-dev")
    print("  - On macOS: brew install weasyprint")
    print("  - On Windows: pip install weasyprint")
    print("\nAlternative: Use browser print to PDF (Ctrl+P) or online PDF generators.")
    sys.exit(1)