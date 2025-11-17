#!/usr/bin/env python3
"""
Trace Visualization Tool

Reads trace log files from the simulator and generates an interactive
web-based timeline visualization using Plotly.

Usage:
    python visualize_trace.py [trace_file]

If no file is specified, it will look for the most recent trace file in data/
"""

import argparse
import re
import sys
from pathlib import Path
from collections import defaultdict
from datetime import datetime

try:
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
    import plotly.express as px
except ImportError:
    print("Error: plotly is required. Install with: pip install plotly")
    sys.exit(1)


class TraceEntry:
    """Represents a single trace entry"""
    def __init__(self, timestamp, event_type, component, event, details, priority=0):
        self.timestamp = int(timestamp)
        self.event_type = event_type.strip()
        self.component = component.strip()
        self.event = event.strip()
        self.details = details.strip()
        self.priority = priority

    def __repr__(self):
        return f"[{self.timestamp}] {self.event_type} {self.component}: {self.event}"


def parse_trace_file(filepath, max_entries=None, sample_rate=1):
    """
    Parse trace log file
    
    Args:
        filepath: Path to trace file
        max_entries: Maximum number of entries to read (None = all)
        sample_rate: Only read every Nth entry (1 = all entries)
    
    Returns:
        List of TraceEntry objects
    """
    entries = []
    # Pattern matches timestamp with spaces before or after the number
    # e.g., both "[         0]" and "[0         ]"
    pattern = re.compile(
        r'\[\s*(\d+)\s*\]\s+\[([^\]]+)\]\s+\[([^\]]+)\]\s+\[([^\]]+)\]\s*(.*?)(?:\(priority=(\d+)\))?$'
    )
    
    print(f"Reading trace file: {filepath}")
    
    with open(filepath, 'r', encoding='utf-8') as f:
        line_count = 0
        for line in f:
            line = line.strip()
            
            # Skip comments and empty lines
            if not line or line.startswith('#') or line.startswith('='):
                continue
            
            line_count += 1
            
            # Apply sampling
            if line_count % sample_rate != 0:
                continue
            
            # Check max entries
            if max_entries and len(entries) >= max_entries:
                break
            
            match = pattern.match(line)
            if match:
                timestamp, event_type, component, event, details, priority = match.groups()
                priority = int(priority) if priority else 0
                entries.append(TraceEntry(timestamp, event_type, component, event, details, priority))
            
            # Progress indicator
            if line_count % 100000 == 0:
                print(f"  Processed {line_count} lines, parsed {len(entries)} entries...")
    
    print(f"Parsed {len(entries)} trace entries from {line_count} lines")
    return entries


def get_component_groups(entries):
    """Group components by prefix for better visualization"""
    components = set(entry.component for entry in entries)
    
    # Group by common prefixes
    groups = defaultdict(list)
    for comp in components:
        # Extract prefix (e.g., "TestTPU_MAC" from "TestTPU_MAC_0_0")
        if '_' in comp:
            parts = comp.split('_')
            # Try different grouping strategies
            if 'MAC' in comp:
                prefix = '_'.join(parts[:2])  # e.g., "TestTPU_MAC"
            elif 'LSU' in comp or 'Bank' in comp:
                prefix = '_'.join(parts[:2])
            else:
                prefix = parts[0]
        else:
            prefix = comp
        
        groups[prefix].append(comp)
    
    return groups


def create_gantt_chart(entries, output_file='trace_gantt.html', title="Component Activity Timeline"):
    """
    Create Gantt chart showing component activity periods
    """
    if not entries:
        return
    
    print(f"Creating Gantt chart...")
    
    # Group events by component
    component_events = defaultdict(list)
    for entry in entries:
        if entry.event_type in ['COMPUTE', 'MAC', 'INSTR', 'MEM_READ', 'MEM_WRITE']:
            component_events[entry.component].append(entry)
    
    # Create activity blocks
    tasks = []
    for component, events in sorted(component_events.items()):
        for event in events:
            tasks.append(dict(
                Task=component,
                Start=event.timestamp,
                Finish=event.timestamp + 1,  # Duration of 1 cycle
                Resource=event.event_type,
                Description=f"{event.event}: {event.details[:50]}"
            ))
    
    if tasks:
        fig = px.timeline(tasks, x_start="Start", x_end="Finish", y="Task", color="Resource",
                         hover_data=["Description"], title=title)
        fig.update_yaxes(categoryorder="total ascending")
        fig.update_layout(height=max(600, len(component_events) * 30), template='plotly_white')
        fig.write_html(output_file)
        print(f"✓ Gantt chart saved to: {output_file}")


def create_pipeline_view(entries, output_file='trace_pipeline.html', title="Pipeline Stages View"):
    """
    Create pipeline stages visualization showing instruction flow
    """
    print(f"Creating pipeline view...")
    
    # Extract pipeline-related events
    pipeline_events = [e for e in entries if e.event_type in ['INSTR', 'COMPUTE', 'MAC', 'QUEUE']]
    
    if not pipeline_events:
        print("No pipeline events found")
        return
    
    # Create figure with instruction trace
    fig = go.Figure()
    
    # Group by instruction type
    instr_by_type = defaultdict(list)
    for event in pipeline_events:
        instr_by_type[event.event_type].append(event)
    
    colors = {'INSTR': '#2ca02c', 'COMPUTE': '#ff7f0e', 'MAC': '#d62728', 'QUEUE': '#9467bd'}
    
    for instr_type, events in instr_by_type.items():
        timestamps = [e.timestamp for e in events]
        components = [e.component for e in events]
        details = [f"{e.event}: {e.details}" for e in events]
        
        fig.add_trace(go.Scatter(
            x=timestamps,
            y=components,
            mode='markers+lines',
            name=instr_type,
            marker=dict(size=10, color=colors.get(instr_type, '#cccccc')),
            text=details,
            hoverinfo='text'
        ))
    
    fig.update_layout(
        title=title,
        xaxis_title='Time (cycles)',
        yaxis_title='Component',
        height=600,
        template='plotly_white'
    )
    
    fig.write_html(output_file)
    print(f"✓ Pipeline view saved to: {output_file}")


def create_timeline_visualization(entries, output_file='trace_timeline.html', 
                                  max_components=50, title="Simulation Trace Timeline"):
    """
    Create interactive timeline visualization
    
    Args:
        entries: List of TraceEntry objects
        output_file: Output HTML file path
        max_components: Maximum number of component rows to show
        title: Chart title
    """
    if not entries:
        print("No entries to visualize")
        return
    
    print(f"Creating visualization with {len(entries)} entries...")
    
    # Get component statistics
    component_counts = defaultdict(int)
    for entry in entries:
        component_counts[entry.component] += 1
    
    # Select top components by activity
    top_components = sorted(component_counts.items(), key=lambda x: x[1], reverse=True)[:max_components]
    selected_components = {comp for comp, _ in top_components}
    
    print(f"Showing top {len(selected_components)} components out of {len(component_counts)}")
    
    # Filter entries (exclude TICK and PROP events)
    filtered_entries = [e for e in entries if e.component in selected_components 
                        and e.event_type not in ['TICK', 'PROP']]
    
    # Assign y-axis positions to components
    component_list = sorted(selected_components)
    component_y = {comp: i for i, comp in enumerate(component_list)}
    
    # Color mapping for event types
    color_map = {
        'TICK': '#1f77b4',
        'EVENT': '#ff7f0e',
        'COMPUTE': '#2ca02c',
        'MEM_READ': '#d62728',
        'MEM_WRITE': '#9467bd',
        'MAC': '#8c564b',
        'QUEUE': '#e377c2',
        'REG': '#7f7f7f',
        'INSTR': '#bcbd22',
        'PROP': '#17becf',
        'STATE': '#aec7e8',
        'COMM': '#ffbb78',
        'CUSTOM': '#98df8a',
    }
    
    # Prepare data for plotting
    x_data = []
    y_data = []
    colors = []
    hover_texts = []
    
    for entry in filtered_entries:
        x_data.append(entry.timestamp)
        y_data.append(component_y[entry.component])
        colors.append(color_map.get(entry.event_type, '#cccccc'))
        hover_texts.append(
            f"<b>{entry.component}</b><br>"
            f"Time: {entry.timestamp}<br>"
            f"Type: {entry.event_type}<br>"
            f"Event: {entry.event}<br>"
            f"Details: {entry.details[:100]}"
        )
    
    # Create scatter plot
    fig = go.Figure()
    
    # Add trace for each event type to create legend
    for event_type, color in color_map.items():
        mask = [e.event_type == event_type for e in filtered_entries]
        if any(mask):
            x_filtered = [x for x, m in zip(x_data, mask) if m]
            y_filtered = [y for y, m in zip(y_data, mask) if m]
            hover_filtered = [h for h, m in zip(hover_texts, mask) if m]
            
            fig.add_trace(go.Scatter(
                x=x_filtered,
                y=y_filtered,
                mode='markers',
                name=event_type,
                marker=dict(
                    size=4,
                    color=color,
                    line=dict(width=0),
                ),
                hovertext=hover_filtered,
                hoverinfo='text',
            ))
    
    # Update layout
    fig.update_layout(
        title=title,
        xaxis=dict(
            title='Simulation Time (cycles)',
            showgrid=True,
            gridcolor='lightgray',
        ),
        yaxis=dict(
            title='Component',
            ticktext=component_list,
            tickvals=list(range(len(component_list))),
            showgrid=True,
            gridcolor='lightgray',
        ),
        height=max(600, len(component_list) * 20),
        hovermode='closest',
        showlegend=True,
        legend=dict(
            orientation="v",
            yanchor="top",
            y=1,
            xanchor="left",
            x=1.01
        ),
        template='plotly_white',
    )
    
    # Save to HTML
    output_path = Path(output_file)
    fig.write_html(str(output_path))
    print(f"\n✓ Visualization saved to: {output_path.absolute()}")
    print(f"  Open in browser: file://{output_path.absolute()}")
    
    # Create additional views
    base_name = output_file.replace('.html', '')
    create_statistics_view(entries, f"{base_name}_stats.html")
    create_gantt_chart(entries, f"{base_name}_gantt.html")
    create_pipeline_view(entries, f"{base_name}_pipeline.html")
    create_timing_diagram(entries, f"{base_name}_timing.html")


def create_timing_diagram(entries, output_file='trace_timing.html', title="Timing Diagram"):
    """
    Create timing diagram showing signal changes over time
    """
    print(f"Creating timing diagram...")
    
    # Extract state changes and important events
    timing_events = [e for e in entries if e.event_type in ['TICK', 'STATE', 'COMPUTE', 'INSTR', 'MAC']]
    
    if not timing_events:
        print("No timing events found")
        return
    
    # Group by component
    component_signals = defaultdict(list)
    for event in timing_events:
        component_signals[event.component].append(event)
    
    fig = go.Figure()
    
    # Create traces for each component (like signal lines)
    for idx, (component, events) in enumerate(sorted(component_signals.items())[:20]):  # Limit to 20 components
        timestamps = [e.timestamp for e in events]
        # Create step function (binary high/low for each event)
        values = [idx + 0.2 + (0.3 if e.event_type != 'TICK' else 0) for e in events]
        
        fig.add_trace(go.Scatter(
            x=timestamps,
            y=values,
            mode='lines+markers',
            name=component,
            line=dict(shape='hv'),  # Step function
            marker=dict(size=4),
            hovertext=[f"{e.event_type}: {e.details[:30]}" for e in events],
            hoverinfo='text'
        ))
    
    fig.update_layout(
        title=title,
        xaxis_title='Time (cycles)',
        yaxis=dict(
            title='Components',
            ticktext=list(sorted(component_signals.keys())[:20]),
            tickvals=list(range(len(list(component_signals.keys())[:20]))),
        ),
        height=800,
        template='plotly_white',
        showlegend=True
    )
    
    fig.write_html(output_file)
    print(f"✓ Timing diagram saved to: {output_file}")


def create_statistics_view(entries, output_file='trace_stats.html'):
    """Create statistical summary visualization"""
    
    # Event type distribution
    event_type_counts = defaultdict(int)
    for entry in entries:
        event_type_counts[entry.event_type] += 1
    
    # Component activity
    component_counts = defaultdict(int)
    for entry in entries:
        component_counts[entry.component] += 1
    
    # Time distribution
    timestamps = [entry.timestamp for entry in entries]
    min_time = min(timestamps) if timestamps else 0
    max_time = max(timestamps) if timestamps else 0
    
    # Create subplots
    fig = make_subplots(
        rows=2, cols=2,
        subplot_titles=(
            'Event Type Distribution',
            'Top 20 Components by Activity',
            'Event Timeline Histogram',
            'Cumulative Events Over Time'
        ),
        specs=[
            [{'type': 'bar'}, {'type': 'bar'}],
            [{'type': 'histogram'}, {'type': 'scatter'}]
        ]
    )
    
    # Event type distribution
    event_types = list(event_type_counts.keys())
    event_counts = list(event_type_counts.values())
    fig.add_trace(
        go.Bar(x=event_types, y=event_counts, name='Event Types'),
        row=1, col=1
    )
    
    # Top components
    top_comps = sorted(component_counts.items(), key=lambda x: x[1], reverse=True)[:20]
    comp_names = [c[0] for c in top_comps]
    comp_counts = [c[1] for c in top_comps]
    fig.add_trace(
        go.Bar(x=comp_names, y=comp_counts, name='Components'),
        row=1, col=2
    )
    
    # Timeline histogram
    fig.add_trace(
        go.Histogram(x=timestamps, nbinsx=100, name='Events'),
        row=2, col=1
    )
    
    # Cumulative events
    sorted_times = sorted(timestamps)
    cumulative = list(range(1, len(sorted_times) + 1))
    fig.add_trace(
        go.Scatter(x=sorted_times, y=cumulative, mode='lines', name='Cumulative'),
        row=2, col=2
    )
    
    fig.update_layout(
        title_text='Trace Statistics Summary',
        showlegend=False,
        height=800,
        template='plotly_white'
    )
    
    fig.update_xaxes(title_text='Event Type', row=1, col=1)
    fig.update_xaxes(title_text='Component', tickangle=45, row=1, col=2)
    fig.update_xaxes(title_text='Time (cycles)', row=2, col=1)
    fig.update_xaxes(title_text='Time (cycles)', row=2, col=2)
    
    fig.update_yaxes(title_text='Count', row=1, col=1)
    fig.update_yaxes(title_text='Count', row=1, col=2)
    fig.update_yaxes(title_text='Frequency', row=2, col=1)
    fig.update_yaxes(title_text='Cumulative Events', row=2, col=2)
    
    output_path = Path(output_file)
    fig.write_html(str(output_path))
    print(f"✓ Statistics saved to: {output_path.absolute()}")


def find_latest_trace():
    """Find the most recent trace file in data/ directory"""
    data_dir = Path('data')
    if not data_dir.exists():
        return None
    
    trace_files = list(data_dir.glob('*.log'))
    if not trace_files:
        return None
    
    # Sort by modification time
    latest = max(trace_files, key=lambda p: p.stat().st_mtime)
    return latest


def main():
    parser = argparse.ArgumentParser(
        description='Visualize simulation trace files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python visualize_trace.py                          # Use latest trace file
  python visualize_trace.py data/trace.log           # Specific file
  python visualize_trace.py -n 10000                 # Limit to 10k entries
  python visualize_trace.py -s 10                    # Sample every 10th entry
  python visualize_trace.py -c 100                   # Show top 100 components
        """
    )
    
    parser.add_argument('trace_file', nargs='?', help='Path to trace log file')
    parser.add_argument('-o', '--output', default='trace_timeline.html',
                       help='Output HTML file (default: trace_timeline.html)')
    parser.add_argument('-n', '--max-entries', type=int,
                       help='Maximum number of entries to read')
    parser.add_argument('-s', '--sample-rate', type=int, default=1,
                       help='Sample every Nth entry (default: 1 = all)')
    parser.add_argument('-c', '--max-components', type=int, default=50,
                       help='Maximum components to show (default: 50)')
    parser.add_argument('-t', '--title', default='Simulation Trace Timeline',
                       help='Chart title')
    
    args = parser.parse_args()
    
    # Determine trace file
    trace_file = args.trace_file
    if not trace_file:
        trace_file = find_latest_trace()
        if not trace_file:
            print("Error: No trace file specified and no trace files found in data/")
            print("Usage: python visualize_trace.py <trace_file>")
            sys.exit(1)
        print(f"Using latest trace file: {trace_file}")
    
    trace_path = Path(trace_file)
    if not trace_path.exists():
        print(f"Error: Trace file not found: {trace_path}")
        sys.exit(1)
    
    # Parse trace
    entries = parse_trace_file(
        trace_path,
        max_entries=args.max_entries,
        sample_rate=args.sample_rate
    )
    
    if not entries:
        print("Error: No valid trace entries found")
        sys.exit(1)
    
    # Create visualization
    create_timeline_visualization(
        entries,
        output_file=args.output,
        max_components=args.max_components,
        title=args.title
    )
    
    print("\n✓ Done! Open the HTML file in your browser to view the visualization.")


if __name__ == '__main__':
    main()
